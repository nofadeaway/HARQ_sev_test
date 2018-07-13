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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "srslte/phch/pdsch.h"
#include "srslte/phch/pusch.h"
#include "srslte/phch/sch.h"
#include "srslte/phch/uci.h"
#include "srslte/common/phy_common.h"
#include "srslte/utils/bit.h"
#include "srslte/utils/debug.h"
#include "srslte/utils/vector.h"

#define SRSLTE_PDSCH_MAX_TDEC_ITERS         4

/* 36.213 Table 8.6.3-1: Mapping of HARQ-ACK offset values and the index signalled by higher layers */
float beta_harq_offset[16] = {2.0, 2.5, 3.125, 4.0, 5.0, 6.250, 8.0, 10.0, 
                           12.625, 15.875, 20.0, 31.0, 50.0, 80.0, 126.0, -1.0};
                                  
/* 36.213 Table 8.6.3-2: Mapping of RI offset values and the index signalled by higher layers */
float beta_ri_offset[16] = {1.25, 1.625, 2.0, 2.5, 3.125, 4.0, 5.0, 6.25, 8.0, 10.0,
                           12.625, 15.875, 20.0, -1.0, -1.0, -1.0};

/* 36.213 Table 8.6.3-3: Mapping of CQI offset values and the index signalled by higher layers */
float beta_cqi_offset[16] = {-1.0, -1.0, 1.125, 1.25, 1.375, 1.625, 1.750, 2.0, 2.25, 2.5, 2.875, 
                             3.125, 3.5, 4.0, 5.0, 6.25};


float srslte_sch_beta_cqi(uint32_t I_cqi) {
  if (I_cqi < 16) {
    return beta_cqi_offset[I_cqi];
  } else {
    return 0;
  }
}
                             
uint32_t srslte_sch_find_Ioffset_ack(float beta) {
  for (int i=0;i<16;i++) {
    if (beta_harq_offset[i] >= beta) {
      return i; 
    }
  }
  return 0;
}
                             
uint32_t srslte_sch_find_Ioffset_ri(float beta) {
  for (int i=0;i<16;i++) {
    if (beta_ri_offset[i] >= beta) {
      return i; 
    }
  }
  return 0;
}
                             
uint32_t srslte_sch_find_Ioffset_cqi(float beta) {
  for (int i=0;i<16;i++) {
    if (beta_cqi_offset[i] >= beta) {
      return i; 
    }
  }
  return 0;
}
                             
int srslte_sch_init(srslte_sch_t *q) {
  int ret = SRSLTE_ERROR_INVALID_INPUTS;
  if (q) {    
    bzero(q, sizeof(srslte_sch_t));
    
    if (srslte_crc_init(&q->crc_tb, SRSLTE_LTE_CRC24A, 24)) {
      fprintf(stderr, "Error initiating CRC\n");
      goto clean;
    }
    if (srslte_crc_init(&q->crc_cb, SRSLTE_LTE_CRC24B, 24)) {
      fprintf(stderr, "Error initiating CRC\n");
      goto clean;
    }

    if (srslte_tcod_init(&q->encoder, SRSLTE_TCOD_MAX_LEN_CB)) {
      fprintf(stderr, "Error initiating Turbo Coder\n");
      goto clean;
    }
    if (srslte_tdec_init(&q->decoder, SRSLTE_TCOD_MAX_LEN_CB)) {
      fprintf(stderr, "Error initiating Turbo Decoder\n");
      goto clean;
    }

    q->max_iterations = SRSLTE_PDSCH_MAX_TDEC_ITERS;
    
    srslte_rm_turbo_gentables();
    
    // Allocate int16 for reception (LLRs)
    q->cb_in = srslte_vec_malloc(sizeof(uint8_t) * (SRSLTE_TCOD_MAX_LEN_CB+8)/8);
    if (!q->cb_in) {
      goto clean;
    }
    
    q->parity_bits = srslte_vec_malloc(sizeof(uint8_t) * (3 * SRSLTE_TCOD_MAX_LEN_CB + 16) / 8);
    if (!q->parity_bits) {
      goto clean;
    }  
    q->temp_g_bits = srslte_vec_malloc(sizeof(uint8_t)*SRSLTE_MAX_PRB*12*12*12);
    if (!q->temp_g_bits) {
      goto clean; 
    }
    bzero(q->temp_g_bits, SRSLTE_MAX_PRB*12*12*12);
    q->ul_interleaver = srslte_vec_malloc(sizeof(uint16_t)*SRSLTE_MAX_PRB*12*12*12);
    if (!q->ul_interleaver) {
      goto clean; 
    }
    if (srslte_uci_cqi_init(&q->uci_cqi)) {
      goto clean;
    }
    
    ret = SRSLTE_SUCCESS;
  }
clean: 
  if (ret == SRSLTE_ERROR) {
    srslte_sch_free(q);
  }
  return ret; 
}

void srslte_sch_free(srslte_sch_t *q) {
  if (q->cb_in) {
    free(q->cb_in);
  }
  if (q->parity_bits) {
    free(q->parity_bits);
  }
  if (q->temp_g_bits) {
    free(q->temp_g_bits);
  }
  if (q->ul_interleaver) {
    free(q->ul_interleaver);
  }
  srslte_tdec_free(&q->decoder);
  srslte_tcod_free(&q->encoder);
  srslte_uci_cqi_free(&q->uci_cqi);
  bzero(q, sizeof(srslte_sch_t));
}

void srslte_sch_set_max_noi(srslte_sch_t *q, uint32_t max_iterations) {
  q->max_iterations = max_iterations;
}

float srslte_sch_average_noi(srslte_sch_t *q) {
  return q->average_nof_iterations; 
}

uint32_t srslte_sch_last_noi(srslte_sch_t *q) {
  return q->nof_iterations;
}


/* Encode a transport block according to 36.212 5.3.2
 *
 */
static int encode_tb_off(srslte_sch_t *q, 
                     srslte_softbuffer_tx_t *softbuffer, srslte_cbsegm_t *cb_segm, 
                     uint32_t Qm, uint32_t rv, uint32_t nof_e_bits,  
                     uint8_t *data, uint8_t *e_bits, uint32_t w_offset) 
{
  uint8_t parity[3] = {0, 0, 0};
  uint32_t par;
  uint32_t i;
  uint32_t cb_len=0, rp=0, wp=0, rlen=0, n_e=0;
  int ret = SRSLTE_ERROR_INVALID_INPUTS; 
  
  if (q            != NULL &&
      e_bits       != NULL &&
      cb_segm      != NULL &&
      softbuffer  != NULL)
  {
  
    if (cb_segm->F) {
      fprintf(stderr, "Error filler bits are not supported. Use standard TBS\n");
      return SRSLTE_ERROR;       
    }

    if (cb_segm->C > softbuffer->max_cb) {
      fprintf(stderr, "Error number of CB (%d) exceeds soft buffer size (%d CBs)\n", cb_segm->C, softbuffer->max_cb);
      return -1; 
    }

    uint32_t Gp = nof_e_bits / Qm;
    
    uint32_t gamma = Gp;
    if (cb_segm->C > 0) {
      gamma = Gp%cb_segm->C;
    }

    if (data) {

      /* Compute transport block CRC */
      par = srslte_crc_checksum_byte(&q->crc_tb, data, cb_segm->tbs);

      /* parity bits will be appended later */
      parity[0] = (par&(0xff<<16))>>16;
      parity[1] = (par&(0xff<<8))>>8;
      parity[2] = par&0xff;
    }
    
    wp = 0;
    rp = 0;
    for (i = 0; i < cb_segm->C; i++) {

      uint32_t cblen_idx; 
      /* Get read lengths */
      if (i < cb_segm->C2) {
        cb_len = cb_segm->K2;
        cblen_idx = cb_segm->K2_idx;
      } else {
        cb_len = cb_segm->K1;
        cblen_idx = cb_segm->K1_idx;
      }
      if (cb_segm->C > 1) {
        rlen = cb_len - 24;
      } else {
        rlen = cb_len;
      }
      if (i <= cb_segm->C - gamma - 1) {
        n_e = Qm * (Gp/cb_segm->C);
      } else {
        n_e = Qm * ((uint32_t) ceilf((float) Gp/cb_segm->C));
      }

      INFO("CB#%d: cb_len: %d, rlen: %d, wp: %d, rp: %d, E: %d\n", i,
          cb_len, rlen, wp, rp, n_e);

      if (data) {

        /* Copy data to another buffer, making space for the Codeblock CRC */
        if (i < cb_segm->C - 1) {
          // Copy data 
          memcpy(q->cb_in, &data[rp/8], rlen * sizeof(uint8_t)/8);
        } else {
          INFO("Last CB, appending parity: %d from %d and 24 to %d\n",
              rlen - 24, rp, rlen - 24);
          
          /* Append Transport Block parity bits to the last CB */
          memcpy(q->cb_in, &data[rp/8], (rlen - 24) * sizeof(uint8_t)/8);
          memcpy(&q->cb_in[(rlen - 24)/8], parity, 3 * sizeof(uint8_t));
        }        
        
        /* Attach Codeblock CRC */
        if (cb_segm->C > 1) {
          srslte_crc_attach_byte(&q->crc_cb, q->cb_in, rlen);
        }

        /* Turbo Encoding */
        srslte_tcod_encode_lut(&q->encoder, q->cb_in, q->parity_bits, cblen_idx);        
      }
      DEBUG("RM cblen_idx=%d, n_e=%d, wp=%d, nof_e_bits=%d\n",cblen_idx, n_e, wp, nof_e_bits);
      
      /* Rate matching */
      if (srslte_rm_turbo_tx_lut(softbuffer->buffer_b[i], q->cb_in, q->parity_bits, 
        &e_bits[(wp+w_offset)/8], cblen_idx, n_e, (wp+w_offset)%8, rv))
      {
        fprintf(stderr, "Error in rate matching\n");
        return SRSLTE_ERROR;
      }
      
      /* Set read/write pointers */
      rp += rlen;
      wp += n_e;
    }
    
    INFO("END CB#%d: wp: %d, rp: %d\n", i, wp, rp);
    ret = SRSLTE_SUCCESS;      
  } 
  return ret; 
}


static int encode_tb(srslte_sch_t *q, 
                     srslte_softbuffer_tx_t *soft_buffer, srslte_cbsegm_t *cb_segm, 
                     uint32_t Qm, uint32_t rv, uint32_t nof_e_bits,  
                     uint8_t *data, uint8_t *e_bits) 
{
  return encode_tb_off(q, soft_buffer, cb_segm, Qm, rv, nof_e_bits, data, e_bits, 0);
}

  


/* Decode a transport block according to 36.212 5.3.2
 *
 */
static int decode_tb(srslte_sch_t *q, 
                     srslte_softbuffer_rx_t *softbuffer, srslte_cbsegm_t *cb_segm, 
                     uint32_t Qm, uint32_t rv, uint32_t nof_e_bits, 
                     int16_t *e_bits, uint8_t *data) 
{
  uint8_t parity[3] = {0, 0, 0};
  uint32_t par_rx, par_tx;
  uint32_t i;
  uint32_t cb_len, rp, wp, rlen, n_e;
  
  if (q            != NULL && 
      data         != NULL &&       
      softbuffer   != NULL &&
      e_bits       != NULL &&
      cb_segm      != NULL)
  {

    if (cb_segm->tbs == 0 || cb_segm->C == 0) {
      return SRSLTE_SUCCESS;
    }
    
    rp = 0;
    rp = 0;
    wp = 0;
    uint32_t Gp = nof_e_bits / Qm;
    uint32_t gamma=Gp;

    if (cb_segm->F) {
      fprintf(stderr, "Error filler bits are not supported. Use standard TBS\n");
      return SRSLTE_ERROR;       
    }

    if (cb_segm->C > softbuffer->max_cb) {
      fprintf(stderr, "Error number of CB (%d) exceeds soft buffer size (%d CBs)\n", cb_segm->C, softbuffer->max_cb);
      return -1; 
    }
    
    if (cb_segm->C>0) {
      gamma = Gp%cb_segm->C;
    }
    
    bool early_stop = true;
    for (i = 0; i < cb_segm->C && early_stop; i++) {

      /* Get read/write lengths */
      uint32_t cblen_idx; 
      if (i < cb_segm->C2) {
        cb_len = cb_segm->K2;
        cblen_idx = cb_segm->K2_idx;
      } else {
        cb_len = cb_segm->K1;
        cblen_idx = cb_segm->K1_idx;
      }
      
      if (cb_segm->C == 1) {
        rlen = cb_len;
      } else {
        rlen = cb_len - 24;
      }

      if (i <= cb_segm->C - gamma - 1) {
        n_e = Qm * (Gp/cb_segm->C);
      } else {
        n_e = Qm * ((uint32_t) ceilf((float) Gp/cb_segm->C));
      }

      /* Rate Unmatching */
      if (srslte_rm_turbo_rx_lut(&e_bits[rp], softbuffer->buffer_f[i], n_e, cblen_idx, rv)) {
        fprintf(stderr, "Error in rate matching\n");
        return SRSLTE_ERROR;
      }

      if (SRSLTE_VERBOSE_ISDEBUG()) {
        char tmpstr[64]; 
        snprintf(tmpstr,64,"rmout_%d.dat",i);
        DEBUG("SAVED FILE %s: Encoded turbo code block %d\n", tmpstr, i);
        srslte_vec_save_file(tmpstr, softbuffer->buffer_f[i], (3*cb_len+12)*sizeof(int16_t));
      }

      /* Turbo Decoding with CRC-based early stopping */
      q->nof_iterations = 0; 
      uint32_t len_crc; 
      srslte_crc_t *crc_ptr; 
      early_stop = false; 

      srslte_tdec_reset(&q->decoder, cb_len);
            
      do {
        srslte_tdec_iteration(&q->decoder, softbuffer->buffer_f[i], cb_len); 
        q->nof_iterations++;
        
        if (cb_segm->C > 1) {
          len_crc = cb_len; 
          crc_ptr = &q->crc_cb; 
        } else {
          len_crc = cb_segm->tbs+24; 
          crc_ptr = &q->crc_tb; 
        }

        srslte_tdec_decision_byte(&q->decoder, q->cb_in, cb_len);
                 
        /* Check Codeblock CRC and stop early if correct */
        if (!srslte_crc_checksum_byte(crc_ptr, q->cb_in, len_crc)) {
          early_stop = true;           
        }
       
      } while (q->nof_iterations < q->max_iterations && !early_stop);
      q->average_nof_iterations = SRSLTE_VEC_EMA((float) q->nof_iterations, q->average_nof_iterations, 0.2);

      INFO("CB#%d: cb_len: %d, rlen: %d, wp: %d, rp: %d, E: %d, n_iters=%d\n", i,
          cb_len, rlen, wp, rp, n_e, q->nof_iterations);
      
            
      // If CB CRC is not correct, early_stop will be false and wont continue with rest of CBs

      /* Copy data to another buffer, removing the Codeblock CRC */
      if (i < cb_segm->C - 1) {
        memcpy(&data[wp/8], q->cb_in, rlen/8 * sizeof(uint8_t));
      } else {        
        /* Append Transport Block parity bits to the last CB */
        memcpy(&data[wp/8], q->cb_in, (rlen - 24)/8 * sizeof(uint8_t));
        memcpy(parity, &q->cb_in[(rlen - 24)/8], 3 * sizeof(uint8_t));
      }
      
      if (SRSLTE_VERBOSE_ISDEBUG()) {
        early_stop = true; 
      }
      
      /* Set read/write pointers */
      wp += rlen;
      rp += n_e;
    }
    
    if (!early_stop) {
      INFO("CB %d failed. TB is erroneous.\n",i-1);
      return SRSLTE_ERROR; 
    } else {
      INFO("END CB#%d: wp: %d, rp: %d\n", i, wp, rp);

      // Compute transport block CRC
      par_rx = srslte_crc_checksum_byte(&q->crc_tb, data, cb_segm->tbs);

      // check parity bits
      par_tx = ((uint32_t) parity[0])<<16 | ((uint32_t) parity[1])<<8 | ((uint32_t) parity[2]);
      
      if (!par_rx) {
        INFO("Warning: Received all-zero transport block\n\n", 0);        
      }

      if (par_rx == par_tx && par_rx) {
        INFO("TB decoded OK\n",i);
        return SRSLTE_SUCCESS;
      } else {
        INFO("Error in TB parity: par_tx=0x%x, par_rx=0x%x\n", par_tx, par_rx);
        return SRSLTE_ERROR;
      }
      
    }
  } else {
    return SRSLTE_ERROR_INVALID_INPUTS;
  }
}

int srslte_dlsch_decode(srslte_sch_t *q, srslte_pdsch_cfg_t *cfg, srslte_softbuffer_rx_t *softbuffer, 
                        int16_t *e_bits, uint8_t *data) 
{
  return decode_tb(q,                    
                   softbuffer, &cfg->cb_segm, 
                   cfg->grant.Qm, cfg->rv, cfg->nbits.nof_bits, 
                   e_bits, data);
}

int srslte_dlsch_encode(srslte_sch_t *q, srslte_pdsch_cfg_t *cfg, srslte_softbuffer_tx_t *softbuffer,
                        uint8_t *data, uint8_t *e_bits) 
{
  return encode_tb(q, 
                   softbuffer, &cfg->cb_segm, 
                   cfg->grant.Qm, cfg->rv, cfg->nbits.nof_bits, 
                   data, e_bits);
}

/* Compute the interleaving function on-the-fly, because it depends on number of RI bits 
 * Profiling show that the computation of this matrix is neglegible. 
 */
static void ulsch_interleave_gen(uint32_t H_prime_total, uint32_t N_pusch_symbs, uint32_t Qm,
                                 uint8_t *ri_present, uint16_t *interleaver_lut)
{
  uint32_t rows = H_prime_total/N_pusch_symbs;
  uint32_t cols = N_pusch_symbs;
  uint32_t idx = 0;
  for(uint32_t j=0; j<rows; j++) {        
    for(uint32_t i=0; i<cols; i++) {
      for(uint32_t k=0; k<Qm; k++) {
        if (ri_present[j*Qm + i*rows*Qm + k]) {
          interleaver_lut[j*Qm + i*rows*Qm + k] = 0; 
        } else {
          interleaver_lut[j*Qm + i*rows*Qm + k] = idx;
          idx++;                  
        }
      }
    }
  }
}

/* UL-SCH channel interleaver according to 5.2.2.8 of 36.212 */
void ulsch_interleave(uint8_t *g_bits, uint32_t Qm, uint32_t H_prime_total, 
                      uint32_t N_pusch_symbs, uint8_t *q_bits, srslte_uci_bit_t *ri_bits, uint32_t nof_ri_bits, 
                      uint8_t *ri_present, uint16_t *inteleaver_lut) 
{
  
  // Prepare ri_bits for fast search using temp_buffer
  if (nof_ri_bits > 0) {
    for (uint32_t i=0;i<nof_ri_bits;i++) {
      ri_present[ri_bits[i].position] = 1;       
    }
  }
  
  // Genearate interleaver table and interleave bits
  ulsch_interleave_gen(H_prime_total, N_pusch_symbs, Qm, ri_present, inteleaver_lut); 
  srslte_bit_interleave(g_bits, q_bits, inteleaver_lut, H_prime_total*Qm);    
  
  // Reset temp_buffer because will be reused next time
  if (nof_ri_bits > 0) {
    for (uint32_t i=0;i<nof_ri_bits;i++) {
      ri_present[ri_bits[i].position] = 0;       
    }
  }
}

/* UL-SCH channel deinterleaver according to 5.2.2.8 of 36.212 */
void ulsch_deinterleave(int16_t *q_bits, uint32_t Qm, uint32_t H_prime_total, 
                        uint32_t N_pusch_symbs, int16_t *g_bits, srslte_uci_bit_t *ri_bits, uint32_t nof_ri_bits, 
                        uint8_t *ri_present, uint16_t *inteleaver_lut) 
{     
  // Prepare ri_bits for fast search using temp_buffer
  if (nof_ri_bits > 0) {
    for (uint32_t i=0;i<nof_ri_bits;i++) {
      ri_present[ri_bits[i].position] = 1;       
    }
  }

  // Generate interleaver table and interleave samples 
  ulsch_interleave_gen(H_prime_total, N_pusch_symbs, Qm, ri_present, inteleaver_lut); 
  srslte_vec_lut_sss(q_bits, inteleaver_lut, g_bits, H_prime_total*Qm);

  // Reset temp_buffer because will be reused next time
  if (nof_ri_bits > 0) {
    for (uint32_t i=0;i<nof_ri_bits;i++) {
      ri_present[ri_bits[i].position] = 0;       
    }
  }
}

int srslte_ulsch_decode(srslte_sch_t *q, srslte_pusch_cfg_t *cfg, srslte_softbuffer_rx_t *softbuffer,
                            int16_t *q_bits, int16_t *g_bits, uint8_t *data) 
{
  srslte_uci_data_t uci_data; 
  bzero(&uci_data, sizeof(srslte_uci_data_t));
  return srslte_ulsch_uci_decode(q, cfg, softbuffer, q_bits, g_bits, data, &uci_data);
}

/* This is done before scrambling */
int srslte_ulsch_uci_decode_ri_ack(srslte_sch_t *q, srslte_pusch_cfg_t *cfg, srslte_softbuffer_rx_t *softbuffer,
                                   int16_t *q_bits, uint8_t *c_seq, srslte_uci_data_t *uci_data) 
{
  int ret = 0; 

  uint32_t Q_prime_ri = 0;
  uint32_t Q_prime_ack = 0;
  
  uint32_t nb_q = cfg->nbits.nof_bits; 
  uint32_t Qm = cfg->grant.Qm; 

  cfg->last_O_cqi = uci_data->uci_cqi_len;
  
  // Deinterleave and decode HARQ bits
  if (uci_data->uci_ack_len > 0) {
    float beta = beta_harq_offset[cfg->uci_cfg.I_offset_ack]; 
    if (cfg->cb_segm.tbs == 0) {
        beta /= beta_cqi_offset[cfg->uci_cfg.I_offset_cqi];
    }
    ret = srslte_uci_decode_ack(cfg, q_bits, c_seq, beta, nb_q/Qm, uci_data->uci_cqi_len, q->ack_ri_bits, &uci_data->uci_ack);
    if (ret < 0) {
      return ret; 
    }
    Q_prime_ack = (uint32_t) ret; 

    // Set zeros to HARQ bits
    for (uint32_t i=0;i<Q_prime_ack;i++) {
      q_bits[q->ack_ri_bits[i].position] = 0;  
    }    
  }
        
  // Deinterleave and decode RI bits
  if (uci_data->uci_ri_len > 0) {
    float beta = beta_ri_offset[cfg->uci_cfg.I_offset_ri]; 
    if (cfg->cb_segm.tbs == 0) {
        beta /= beta_cqi_offset[cfg->uci_cfg.I_offset_cqi];
    }
    ret = srslte_uci_decode_ri(cfg, q_bits, c_seq, beta, nb_q/Qm, uci_data->uci_cqi_len, q->ack_ri_bits, &uci_data->uci_ri);
    if (ret < 0) {
      return ret; 
    }
    Q_prime_ri = (uint32_t) ret;     
  }
  
  q->nof_ri_ack_bits = Q_prime_ri; 
  
  return SRSLTE_SUCCESS;
}

int srslte_ulsch_uci_decode(srslte_sch_t *q, srslte_pusch_cfg_t *cfg, srslte_softbuffer_rx_t *softbuffer,
                            int16_t *q_bits, int16_t *g_bits, uint8_t *data, srslte_uci_data_t *uci_data) 
{
  int ret = 0; 
  
  uint32_t Q_prime_ri = q->nof_ri_ack_bits; 
  uint32_t Q_prime_cqi = 0; 
  uint32_t e_offset = 0;

  uint32_t nb_q = cfg->nbits.nof_bits; 
  uint32_t Qm = cfg->grant.Qm; 

  // Deinterleave data and CQI in ULSCH 
  ulsch_deinterleave(q_bits, Qm, nb_q/Qm, cfg->nbits.nof_symb, g_bits, q->ack_ri_bits, Q_prime_ri*Qm, 
                     q->temp_g_bits, q->ul_interleaver);
  
  // Decode CQI (multiplexed at the front of ULSCH)
  if (uci_data->uci_cqi_len > 0) {
    struct timeval t[3];
    gettimeofday(&t[1], NULL);
    ret = srslte_uci_decode_cqi_pusch(&q->uci_cqi, cfg, g_bits, 
                                      beta_cqi_offset[cfg->uci_cfg.I_offset_cqi], 
                                      Q_prime_ri, uci_data->uci_cqi_len,
                                      uci_data->uci_cqi, &uci_data->cqi_ack);
    gettimeofday(&t[2], NULL);
    get_time_interval(t);
    printf("texec=%ld us\n", t[0].tv_usec);
    
    if (ret < 0) {
      return ret; 
    }
    Q_prime_cqi = (uint32_t) ret; 
  }
  
  e_offset += Q_prime_cqi*Qm;

  // Decode ULSCH
  if (cfg->cb_segm.tbs > 0) {
    uint32_t G = nb_q/Qm - Q_prime_ri - Q_prime_cqi;     
    ret = decode_tb(q, softbuffer, &cfg->cb_segm, 
                   Qm, cfg->rv, G*Qm, 
                   &g_bits[e_offset], data);
    if (ret) {
      return ret; 
    }
  }
  return SRSLTE_SUCCESS; 
}

int srslte_ulsch_encode(srslte_sch_t *q, srslte_pusch_cfg_t *cfg, srslte_softbuffer_tx_t *softbuffer,
                        uint8_t *data, uint8_t *g_bits, uint8_t *q_bits) 
{
  srslte_uci_data_t uci_data; 
  bzero(&uci_data, sizeof(srslte_uci_data_t));
  return srslte_ulsch_uci_encode(q, cfg, softbuffer, data, uci_data, g_bits, q_bits);
}

int srslte_ulsch_uci_encode(srslte_sch_t *q, 
                            srslte_pusch_cfg_t *cfg, srslte_softbuffer_tx_t *softbuffer,
                            uint8_t *data, srslte_uci_data_t uci_data, 
                            uint8_t *g_bits, uint8_t *q_bits) 
{
  int ret; 
   
  uint32_t e_offset = 0;
  uint32_t Q_prime_cqi = 0; 
  uint32_t Q_prime_ack = 0;
  uint32_t Q_prime_ri = 0;

  uint32_t nb_q = cfg->nbits.nof_bits; 
  uint32_t Qm = cfg->grant.Qm; 
  
  // Encode RI
  if (uci_data.uci_ri_len > 0) {
    float beta = beta_ri_offset[cfg->uci_cfg.I_offset_ri]; 
    if (cfg->cb_segm.tbs == 0) {
        beta /= beta_cqi_offset[cfg->uci_cfg.I_offset_cqi];
    }
    ret = srslte_uci_encode_ri(cfg, uci_data.uci_ri, uci_data.uci_cqi_len, beta, nb_q/Qm, q->ack_ri_bits);
    if (ret < 0) {
      return ret; 
    }
    Q_prime_ri = (uint32_t) ret; 
  }
  
  // Encode CQI
  cfg->last_O_cqi = uci_data.uci_cqi_len;
  if (uci_data.uci_cqi_len > 0) {
    ret = srslte_uci_encode_cqi_pusch(&q->uci_cqi, cfg, 
                                      uci_data.uci_cqi, uci_data.uci_cqi_len, 
                                      beta_cqi_offset[cfg->uci_cfg.I_offset_cqi], 
                                      Q_prime_ri, q->temp_g_bits);
    if (ret < 0) {
      return ret; 
    }
    Q_prime_cqi = (uint32_t) ret; 
    srslte_bit_pack_vector(q->temp_g_bits, g_bits, Q_prime_cqi*Qm);
    // Reset the buffer because will be reused in ulsch_interleave
    bzero(q->temp_g_bits, Q_prime_cqi*Qm);
  }
  
  e_offset += Q_prime_cqi*Qm;

  // Encode UL-SCH
  if (cfg->cb_segm.tbs > 0) {
    uint32_t G = nb_q/Qm - Q_prime_ri - Q_prime_cqi;     
    ret = encode_tb_off(q, softbuffer, &cfg->cb_segm, 
                    Qm, cfg->rv, G*Qm, 
                    data, &g_bits[e_offset/8], e_offset%8);
    if (ret) {
      return ret; 
    }    
  } 
  
  // Interleave UL-SCH (and RI and CQI)
  ulsch_interleave(g_bits, Qm, nb_q/Qm, cfg->nbits.nof_symb, q_bits, q->ack_ri_bits, Q_prime_ri*Qm, 
                   q->temp_g_bits, q->ul_interleaver);
  
  // Encode (and interleave) ACK
  if (uci_data.uci_ack_len > 0) {
    float beta = beta_harq_offset[cfg->uci_cfg.I_offset_ack]; 
    if (cfg->cb_segm.tbs == 0) {
        beta /= beta_cqi_offset[cfg->uci_cfg.I_offset_cqi];
    }
    ret = srslte_uci_encode_ack(cfg, uci_data.uci_ack, uci_data.uci_cqi_len, beta, nb_q/Qm, &q->ack_ri_bits[Q_prime_ri*Qm]);
    if (ret < 0) {
      return ret; 
    }
    Q_prime_ack = (uint32_t) ret; 
  }
  
  q->nof_ri_ack_bits = (Q_prime_ack+Q_prime_ri)*Qm;
  
  for (uint32_t i=0;i<q->nof_ri_ack_bits;i++) {
    uint32_t p = q->ack_ri_bits[i].position;
    if (p < nb_q) {
      if (q->ack_ri_bits[i].type == UCI_BIT_1) {
        q_bits[p/8] |= (1<<(7-p%8));
      } else {
        q_bits[p/8] &= ~(1<<(7-p%8));
      }
    } else {
      fprintf(stderr, "Invalid RI/ACK bit position %d. Max bits=%d\n", p, nb_q);
    }    
  }
    
  
  INFO("Q_prime_ack=%d, Q_prime_cqi=%d, Q_prime_ri=%d\n",Q_prime_ack, Q_prime_cqi, Q_prime_ri);  

  return SRSLTE_SUCCESS;
}

