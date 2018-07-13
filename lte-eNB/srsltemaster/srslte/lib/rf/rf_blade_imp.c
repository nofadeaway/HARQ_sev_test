/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsLTE library.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <libbladeRF.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "srslte/srslte.h"
#include "rf_blade_imp.h"
#include "srslte/rf/rf.h"


#define CONVERT_BUFFER_SIZE 240*1024

typedef struct {
  struct bladerf *dev; 
  uint32_t rx_rate;
  uint32_t tx_rate;
  int16_t rx_buffer[CONVERT_BUFFER_SIZE]; 
  int16_t tx_buffer[CONVERT_BUFFER_SIZE]; 
  bool rx_stream_enabled; 
  bool tx_stream_enabled; 
} rf_blade_handler_t;

srslte_rf_error_handler_t blade_error_handler = NULL; 

void rf_blade_suppress_stdout(void *h) {
  bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_SILENT);
}

void rf_blade_register_error_handler(void *notused, srslte_rf_error_handler_t new_handler)
{
  new_handler = blade_error_handler;
}

bool rf_blade_rx_wait_lo_locked(void *h)
{
  usleep(1000);
  return true; 
}

const unsigned int num_buffers    = 256;
const unsigned int ms_buffer_size_rx = 1024; 
const unsigned int buffer_size_tx = 1024;  
const unsigned int num_transfers  = 32;
const unsigned int timeout_ms     = 4000;

 
char* rf_blade_devname(void* h)
{
  return DEVNAME;
}

int rf_blade_start_tx_stream(void *h)
{
  int status; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  
  status = bladerf_sync_config(handler->dev,
                                BLADERF_MODULE_TX,
                                BLADERF_FORMAT_SC16_Q11_META,
                                num_buffers,
                                buffer_size_tx,
                                num_transfers,
                                timeout_ms);
  if (status != 0) {
    fprintf(stderr, "Failed to configure TX sync interface: %s\n", bladerf_strerror(status));
    return status; 
  }
  status = bladerf_enable_module(handler->dev, BLADERF_MODULE_TX, true);
  if (status != 0) {
    fprintf(stderr, "Failed to enable TX module: %s\n", bladerf_strerror(status));
    return status;
  }
  handler->tx_stream_enabled = true; 
  return 0;
}

int rf_blade_start_rx_stream(void *h)
{
  int status; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
    
  /* Configure the device's RX module for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
  uint32_t buffer_size_rx = ms_buffer_size_rx*(handler->rx_rate/1000/1024); 
  
  status = bladerf_sync_config(handler->dev,
                                BLADERF_MODULE_RX,
                                BLADERF_FORMAT_SC16_Q11_META,
                                num_buffers,
                                buffer_size_rx,
                                num_transfers,
                                timeout_ms);
  if (status != 0) {
    fprintf(stderr, "Failed to configure RX sync interface: %s\n", bladerf_strerror(status));
    return status;
  }
  status = bladerf_sync_config(handler->dev,
                                BLADERF_MODULE_TX,
                                BLADERF_FORMAT_SC16_Q11_META,
                                num_buffers,
                                buffer_size_tx,
                                num_transfers,
                                timeout_ms);
  if (status != 0) {
    fprintf(stderr, "Failed to configure TX sync interface: %s\n", bladerf_strerror(status));
    return status; 
  }
  status = bladerf_enable_module(handler->dev, BLADERF_MODULE_RX, true);
  if (status != 0) {
    fprintf(stderr, "Failed to enable RX module: %s\n", bladerf_strerror(status));
    return status;
  }
  status = bladerf_enable_module(handler->dev, BLADERF_MODULE_TX, true);
  if (status != 0) {
    fprintf(stderr, "Failed to enable TX module: %s\n", bladerf_strerror(status));
    return status;
  }
  handler->rx_stream_enabled = true; 
  return 0;
}

int rf_blade_stop_rx_stream(void *h)
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  int status = bladerf_enable_module(handler->dev, BLADERF_MODULE_RX, false);
  if (status != 0) {
    fprintf(stderr, "Failed to enable RX module: %s\n", bladerf_strerror(status));
    return status;
  }
  status = bladerf_enable_module(handler->dev, BLADERF_MODULE_TX, false);
  if (status != 0) {
    fprintf(stderr, "Failed to enable TX module: %s\n", bladerf_strerror(status));
    return status;
  }
  handler->rx_stream_enabled = false;
  handler->tx_stream_enabled = false; 
  return 0;
}

void rf_blade_flush_buffer(void *h)
{
}

bool rf_blade_has_rssi(void *h) 
{
  return false; 
}

float rf_blade_get_rssi(void *h) 
{
  return 0;
}

int rf_blade_open(char *args, void **h)
{
  *h = NULL; 
  
  rf_blade_handler_t *handler = (rf_blade_handler_t*) malloc(sizeof(rf_blade_handler_t));
  if (!handler) {
    perror("malloc");
    return -1; 
  }
  *h = handler; 

  printf("Opening bladeRF...\n");
  int status = bladerf_open(&handler->dev, args);
  if (status) {
    fprintf(stderr, "Unable to open device: %s\n", bladerf_strerror(status));
    return status;
  }
  
  //bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);
  
  /* Configure the gains of the RX LNA and RX VGA1*/
  status = bladerf_set_lna_gain(handler->dev, BLADERF_LNA_GAIN_MAX);
  if (status != 0) {
    fprintf(stderr, "Failed to set RX LNA gain: %s\n", bladerf_strerror(status));
    return status;
  }
  status = bladerf_set_rxvga1(handler->dev, 27);
  if (status != 0) {
    fprintf(stderr, "Failed to set RX VGA1 gain: %s\n", bladerf_strerror(status));
    return status;
  }
  status = bladerf_set_txvga1(handler->dev, BLADERF_TXVGA1_GAIN_MAX);
  if (status != 0) {
    fprintf(stderr, "Failed to set TX VGA1 gain: %s\n", bladerf_strerror(status));
    return status;
  }
  handler->rx_stream_enabled = false; 
  handler->tx_stream_enabled = false; 
  return 0;
}


int rf_blade_close(void *h)
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  bladerf_close(handler->dev);
  return 0;
}

void rf_blade_set_master_clock_rate(void *h, double rate) 
{
}

bool rf_blade_is_master_clock_dynamic(void *h) 
{
  return true; 
}

double rf_blade_set_rx_srate(void *h, double freq)
{
  uint32_t bw;
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  int status = bladerf_set_sample_rate(handler->dev, BLADERF_MODULE_RX, (uint32_t) freq, &handler->rx_rate);
  if (status != 0) {
    fprintf(stderr, "Failed to set samplerate = %u: %s\n", (uint32_t) freq, bladerf_strerror(status));
    return -1;
  }
  if (handler->rx_rate < 2000000) { 
    status = bladerf_set_bandwidth(handler->dev, BLADERF_MODULE_RX, handler->rx_rate, &bw);
    if (status != 0) {
      fprintf(stderr, "Failed to set bandwidth = %u: %s\n", handler->rx_rate, bladerf_strerror(status));
      return -1;
    }
  } else {
    status = bladerf_set_bandwidth(handler->dev, BLADERF_MODULE_RX, handler->rx_rate*0.8, &bw);
    if (status != 0) {
      fprintf(stderr, "Failed to set bandwidth = %u: %s\n", handler->rx_rate, bladerf_strerror(status));
      return -1;
    }
  }
  printf("Set RX sampling rate %.2f Mhz, filter BW: %.2f Mhz\n", (float) handler->rx_rate/1e6, (float) bw/1e6);
  return (double) handler->rx_rate;
}

double rf_blade_set_tx_srate(void *h, double freq)
{
  uint32_t bw;
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  int status = bladerf_set_sample_rate(handler->dev, BLADERF_MODULE_TX, (uint32_t) freq, &handler->tx_rate);
  if (status != 0) {
    fprintf(stderr, "Failed to set samplerate = %u: %s\n", (uint32_t) freq, bladerf_strerror(status));
    return -1;
  }  
  status = bladerf_set_bandwidth(handler->dev, BLADERF_MODULE_TX, handler->tx_rate, &bw);
  if (status != 0) {
    fprintf(stderr, "Failed to set bandwidth = %u: %s\n", handler->tx_rate, bladerf_strerror(status));
    return -1;
  }
  return (double) handler->tx_rate;
}

double rf_blade_set_rx_gain(void *h, double gain)
{
  int status; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;  
  status = bladerf_set_rxvga2(handler->dev, (int) gain);
  if (status != 0) {
    fprintf(stderr, "Failed to set RX VGA2 gain: %s\n", bladerf_strerror(status));
    return -1;
  }
  return rf_blade_get_rx_gain(h);
}

double rf_blade_set_tx_gain(void *h, double gain)
{
  int status; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;  
  status = bladerf_set_txvga2(handler->dev, (int) gain);
  if (status != 0) {
    fprintf(stderr, "Failed to set TX VGA2 gain: %s\n", bladerf_strerror(status));
    return -1;
  }
  return rf_blade_get_tx_gain(h);
}

double rf_blade_get_rx_gain(void *h)
{
  int status; 
  int gain; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;  
  status = bladerf_get_rxvga2(handler->dev, &gain);
  if (status != 0) {
    fprintf(stderr, "Failed to get RX VGA2 gain: %s\n",
            bladerf_strerror(status));
    return -1;
  }
  return gain; // Add rxvga1 and LNA
}

double rf_blade_get_tx_gain(void *h)
{
  int status; 
  int gain; 
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;  
  status = bladerf_get_txvga2(handler->dev, &gain);
  if (status != 0) {
    fprintf(stderr, "Failed to get TX VGA2 gain: %s\n",
            bladerf_strerror(status));
    return -1;
  }
  return gain; // Add txvga1
}

double rf_blade_set_rx_freq(void *h, double freq)
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  uint32_t f_int = (uint32_t) round(freq);
  int status = bladerf_set_frequency(handler->dev, BLADERF_MODULE_RX, f_int);
  if (status != 0) {
    fprintf(stderr, "Failed to set samplerate = %u: %s\n",
            (uint32_t) freq, bladerf_strerror(status));
    return -1;
  }
  f_int=0;
  bladerf_get_frequency(handler->dev, BLADERF_MODULE_RX, &f_int);
  printf("set RX frequency to %u\n", f_int);
  
  return freq;
}

double rf_blade_set_tx_freq(void *h, double freq)
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  uint32_t f_int = (uint32_t) round(freq);
  int status = bladerf_set_frequency(handler->dev, BLADERF_MODULE_TX, f_int);
  if (status != 0) {
    fprintf(stderr, "Failed to set samplerate = %u: %s\n",
            (uint32_t) freq, bladerf_strerror(status));
    return -1;
  }
  
  f_int=0;
  bladerf_get_frequency(handler->dev, BLADERF_MODULE_TX, &f_int);
  printf("set TX frequency to %u\n", f_int);
  return freq;
}

void rf_blade_set_tx_cal(void *h, srslte_rf_cal_t *cal) {
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  bladerf_set_correction(handler->dev, BLADERF_MODULE_TX, BLADERF_CORR_FPGA_PHASE, cal->dc_gain);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_TX, BLADERF_CORR_FPGA_GAIN, cal->dc_phase);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_TX, BLADERF_CORR_LMS_DCOFF_I, cal->iq_i);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_TX, BLADERF_CORR_LMS_DCOFF_Q, cal->iq_q);  
}

void rf_blade_set_rx_cal(void *h, srslte_rf_cal_t *cal) {
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  bladerf_set_correction(handler->dev, BLADERF_MODULE_RX, BLADERF_CORR_FPGA_PHASE, cal->dc_gain);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_RX, BLADERF_CORR_FPGA_GAIN, cal->dc_phase);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_RX, BLADERF_CORR_LMS_DCOFF_I, cal->iq_i);
  bladerf_set_correction(handler->dev, BLADERF_MODULE_RX, BLADERF_CORR_LMS_DCOFF_Q, cal->iq_q);  
}


static void timestamp_to_secs(uint32_t rate, uint64_t timestamp, time_t *secs, double *frac_secs) {
  double totalsecs = (double) timestamp/rate;
  time_t secs_i = (time_t) totalsecs;
  if (secs) {
    *secs = secs_i;
  }
  if (frac_secs) {
    *frac_secs = totalsecs-secs_i;
  }
}

static void secs_to_timestamps(uint32_t rate, time_t secs, double frac_secs, uint64_t *timestamp) {
  double totalsecs = (double) secs + frac_secs;
  if (timestamp) {
    *timestamp = rate * totalsecs;
  }
}

void rf_blade_get_time(void *h, time_t *secs, double *frac_secs) 
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  struct bladerf_metadata meta;
  
  int status = bladerf_get_timestamp(handler->dev, BLADERF_MODULE_RX, &meta.timestamp);
  if (status != 0) {
      fprintf(stderr, "Failed to get current RX timestamp: %s\n",
              bladerf_strerror(status));
  }
  timestamp_to_secs(handler->rx_rate, meta.timestamp, secs, frac_secs);
}

int rf_blade_recv_with_time(void *h,
                    void *data,
                    uint32_t nsamples,
                    bool blocking,
                    time_t *secs,
                    double *frac_secs) 
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  struct bladerf_metadata meta;
  int status; 
  
  memset(&meta, 0, sizeof(meta));
  meta.flags = BLADERF_META_FLAG_RX_NOW;
  
  if (2*nsamples > CONVERT_BUFFER_SIZE) {
    fprintf(stderr, "RX failed: nsamples exceeds buffer size (%d>%d)\n", nsamples, CONVERT_BUFFER_SIZE);
    return -1;
  }
  status = bladerf_sync_rx(handler->dev, handler->rx_buffer, nsamples, &meta, 2000);
  if (status) {
    fprintf(stderr, "RX failed: %s\n\n", bladerf_strerror(status));
    return -1;
  } else if (meta.status & BLADERF_META_STATUS_OVERRUN) {
    if (blade_error_handler) {
      srslte_rf_error_t error; 
      error.opt = meta.actual_count;
      error.type = SRSLTE_RF_ERROR_OVERFLOW;
      blade_error_handler(error);
    } else {     
      fprintf(stderr, "Overrun detected in scheduled RX. "
            "%u valid samples were read.\n\n", meta.actual_count);
    }
  }
  
  timestamp_to_secs(handler->rx_rate, meta.timestamp, secs, frac_secs);
  srslte_vec_convert_if(handler->rx_buffer, data, 2048, 2*nsamples);
  
  return nsamples;
}
                   
int rf_blade_send_timed(void *h,
                     void *data,
                     int nsamples,
                     time_t secs,
                     double frac_secs,                      
                     bool has_time_spec,
                     bool blocking,
                     bool is_start_of_burst,
                     bool is_end_of_burst) 
{
  rf_blade_handler_t *handler = (rf_blade_handler_t*) h;
  struct bladerf_metadata meta;
  int status; 
  
  if (!handler->tx_stream_enabled) {
    rf_blade_start_tx_stream(h);
  }
  
  if (2*nsamples > CONVERT_BUFFER_SIZE) {
    fprintf(stderr, "TX failed: nsamples exceeds buffer size (%d>%d)\n", nsamples, CONVERT_BUFFER_SIZE);
    return -1;
  }

  srslte_vec_convert_fi(data, handler->tx_buffer, 2048, 2*nsamples);
  
  memset(&meta, 0, sizeof(meta));
  if (is_start_of_burst) {
    if (has_time_spec) {
      secs_to_timestamps(handler->tx_rate, secs, frac_secs, &meta.timestamp);
    } else {
      meta.flags |= BLADERF_META_FLAG_TX_NOW;
    }
    meta.flags |= BLADERF_META_FLAG_TX_BURST_START;
  }
  if (is_end_of_burst) {
    meta.flags |= BLADERF_META_FLAG_TX_BURST_END;
  }
  srslte_rf_error_t error; 
  bzero(&error, sizeof(srslte_rf_error_t));
  
  status = bladerf_sync_tx(handler->dev, handler->tx_buffer, nsamples, &meta, 2000);
  if (status == BLADERF_ERR_TIME_PAST) {
    if (blade_error_handler) {
      error.type = SRSLTE_RF_ERROR_LATE;
      blade_error_handler(error);
    } else {
      fprintf(stderr, "TX failed: %s\n", bladerf_strerror(status));      
    }
  } else if (status) {
    fprintf(stderr, "TX failed: %s\n", bladerf_strerror(status));
    return status;
  } else if (meta.status == BLADERF_META_STATUS_UNDERRUN) {
    if (blade_error_handler) {
      error.type = SRSLTE_RF_ERROR_UNDERFLOW;
      blade_error_handler(error);
    } else {
      fprintf(stderr, "TX warning: underflow detected.\n");
    }
  }
  
  return nsamples;
}

