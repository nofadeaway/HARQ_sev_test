/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

/******************************************************************************
 * File:        ue.h
 * Description: Top-level UE class. Creates and links all
 *              layers and helpers.
 *****************************************************************************/

#ifndef UE_H
#define UE_H

#include <stdarg.h>
#include <string>
#include <pthread.h>

#include "radio/radio.h"
#include "phy/phy.h"
#include "mac/mac.h"
#include "upper/rlc.h"
#include "upper/pdcp.h"
#include "upper/rrc.h"
#include "upper/nas.h"
#include "upper/gw.h"
#include "upper/usim.h"

#include "common/buffer_pool.h"
#include "common/interfaces.h"
#include "common/logger.h"
#include "common/log_filter.h"

#include "ue_metrics_interface.h"

namespace srsue {

/*******************************************************************************
  UE Parameters
*******************************************************************************/

typedef struct {
  float         dl_freq;
  float         ul_freq;
  float         rx_gain;
  float         tx_gain;
  std::string   device_name; 
  std::string   device_args; 
  std::string   time_adv_nsamples; 
  std::string   burst_preamble; 
}rf_args_t;

typedef struct {
  bool          enable;
  std::string   filename;
}pcap_args_t;

typedef struct {
  bool          enable;
  std::string   phy_filename;
  std::string   radio_filename;
}trace_args_t;

typedef struct {
  std::string   phy_level;
  std::string   mac_level;
  std::string   rlc_level;
  std::string   pdcp_level;
  std::string   rrc_level;
  std::string   gw_level;
  std::string   nas_level;
  std::string   usim_level;
  std::string   all_level;
  int           phy_hex_limit;
  int           mac_hex_limit;
  int           rlc_hex_limit;
  int           pdcp_hex_limit;
  int           rrc_hex_limit;
  int           gw_hex_limit;
  int           nas_hex_limit;
  int           usim_hex_limit;
  int           all_hex_limit;
  std::string   filename;
}log_args_t;

typedef struct {
  bool          enable;
}gui_args_t;

typedef struct {
  phy_args_t phy; 
  float      metrics_period_secs;
  bool pregenerate_signals;
}expert_args_t;

typedef struct {
  rf_args_t     rf;
  rf_cal_t      rf_cal; 
  pcap_args_t   pcap;
  trace_args_t  trace;
  log_args_t    log;
  gui_args_t    gui;
  usim_args_t   usim;
  expert_args_t expert;
}all_args_t;

/*******************************************************************************
  Main UE class
*******************************************************************************/

class ue
    :public ue_interface
    ,public ue_metrics_interface
{
public:
  static ue* get_instance(void);
  static void cleanup(void);

  bool init(all_args_t *args_);
  void stop();
  bool is_attached();
  void start_plot();
  
  static void rf_msg(srslte_rf_error_t error);
  void handle_rf_msg(srslte_rf_error_t error);

  // UE metrics interface
  bool get_metrics(ue_metrics_t &m);

  void pregenerate_signals(bool enable);
  
  // Testing
  void test_con_restablishment(); 
  

private:
  static ue *instance;
  ue();
  ~ue();

  srslte::radio radio;
  srsue::phy        phy;
  srsue::mac        mac;
  srslte::mac_pcap   mac_pcap;
  srsue::rlc        rlc;
  srsue::pdcp       pdcp;
  srsue::rrc        rrc;
  srsue::nas        nas;
  srsue::gw         gw;
  srsue::usim       usim;

  srslte::logger     logger;
  srslte::log_filter rf_log;
  srslte::log_filter phy_log;
  srslte::log_filter mac_log;
  srslte::log_filter rlc_log;
  srslte::log_filter pdcp_log;
  srslte::log_filter rrc_log;
  srslte::log_filter nas_log;
  srslte::log_filter gw_log;
  srslte::log_filter usim_log;

  srslte::buffer_pool *pool;

  all_args_t       *args;
  bool              started;
  rf_metrics_t     rf_metrics;

  srslte::LOG_LEVEL_ENUM level(std::string l);
  
  bool check_srslte_version();
};

} // namespace srsue

#endif // UE_H
  
