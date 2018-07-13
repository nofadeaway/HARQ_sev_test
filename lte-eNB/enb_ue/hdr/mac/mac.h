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

#ifndef MAC_H
#define MAC_H

#include "common/log.h"
#include "phy/phy.h"
#include "mac/dl_harq.h"
#include "mac/ul_harq.h"
#include "common/timers.h"
#include "mac/mac_metrics.h"
#include "mac/proc_ra.h"
#include "mac/proc_sr.h"
#include "mac/proc_bsr.h"
#include "mac/proc_phr.h"
#include "mac/mux.h"
#include "mac/demux.h"
#include "common/mac_pcap.h"
#include "common/mac_interface.h"
#include "common/tti_sync_cv.h"
#include "common/threads.h"

namespace srsue {
  
class mac
    :public mac_interface_phy
    ,public mac_interface_rrc
    ,public srslte::mac_interface_timers
    ,public thread
    ,public srslte::timer_callback
{
public:
  mac();
  bool init(phy_interface_mac *phy, rlc_interface_mac *rlc, rrc_interface_mac* rrc, srslte::log *log_h);
  void stop();

  void get_metrics(mac_metrics_t &m);

  /******** Interface from PHY (PHY -> MAC) ****************/ 
  /* see mac_interface.h for comments */
  void new_grant_ul(mac_grant_t grant, tb_action_ul_t *action);
  void new_grant_ul_ack(mac_grant_t grant, bool ack, tb_action_ul_t *action);
  void harq_recv(uint32_t tti, bool ack, tb_action_ul_t *action);
  void new_grant_dl(mac_grant_t grant, tb_action_dl_t *action);
  void tb_decoded(bool ack, srslte_rnti_type_t rnti_type, uint32_t harq_pid);
  void bch_decoded_ok(uint8_t *payload, uint32_t len);
  void pch_decoded_ok(uint32_t len);    
  void tti_clock(uint32_t tti);

  
  /******** Interface from RLC (RLC -> MAC) ****************/ 
  void bcch_start_rx(); 
  void bcch_stop_rx(); 
  void bcch_start_rx(int si_window_start, int si_window_length);
  void pcch_start_rx(); 
  void pcch_stop_rx(); 
  void setup_lcid(uint32_t lcid, uint32_t lcg, uint32_t priority, int PBR_x_tti, uint32_t BSD);
  void reconfiguration(); 
  void reset(); 

  /******** set/get MAC configuration  ****************/ 
  void set_config(mac_cfg_t *mac_cfg);
  void get_config(mac_cfg_t *mac_cfg);
  void set_config_main(LIBLTE_RRC_MAC_MAIN_CONFIG_STRUCT *main_cfg);
  void set_config_rach(LIBLTE_RRC_RACH_CONFIG_COMMON_STRUCT *rach_cfg, uint32_t prach_config_index);
  void set_config_sr(LIBLTE_RRC_SCHEDULING_REQUEST_CONFIG_STRUCT *sr_cfg);
  void set_contention_id(uint64_t uecri);
  
  void get_rntis(ue_rnti_t *rntis);
  
  void timer_expired(uint32_t timer_id); 
  void start_pcap(srslte::mac_pcap* pcap);
  
  srslte::timers::timer*   get(uint32_t timer_id);
  u_int32_t                get_unique_id();
  
  uint32_t get_current_tti();
      
  enum {
    HARQ_RTT, 
    TIME_ALIGNMENT,
    CONTENTION_TIMER,
    BSR_TIMER_PERIODIC,
    BSR_TIMER_RETX,
    PHR_TIMER_PERIODIC,
    PHR_TIMER_PROHIBIT,
    NOF_MAC_TIMERS
  } mac_timers_t; 
  
  static const int MAC_NOF_UPPER_TIMERS = 20; 
  
private:  
  void run_thread(); 
  
  static const int MAC_MAIN_THREAD_PRIO = 5; 
  static const int MAC_PDU_THREAD_PRIO  = 6;

  // Interaction with PHY 
  srslte::tti_sync_cv   ttisync; 
  phy_interface_mac    *phy_h; 
  rlc_interface_mac    *rlc_h; 
  rrc_interface_mac    *rrc_h; 
  srslte::log          *log_h;
  
  // MAC configuration 
  mac_cfg_t     config; 

  // UE-specific RNTIs 
  ue_rnti_t     uernti; 
  
  uint32_t      tti; 
  bool          started; 
  bool          is_synchronized; 
  uint16_t      last_temporal_crnti;
  uint16_t      phy_rnti;
  
  /* Multiplexing/Demultiplexing Units */
  mux           mux_unit; 
  demux         demux_unit; 
  
  /* DL/UL HARQ */  
  dl_harq_entity dl_harq; 
  ul_harq_entity ul_harq; 
  
  /* MAC Uplink-related Procedures */
  ra_proc       ra_procedure;
  sr_proc       sr_procedure; 
  bsr_proc      bsr_procedure; 
  phr_proc      phr_procedure; 
  
  /* Buffers for PCH reception (not included in DL HARQ) */
  const static uint32_t  pch_payload_buffer_sz = 8*1024;
  srslte_softbuffer_rx_t pch_softbuffer;
  uint8_t                pch_payload_buffer[pch_payload_buffer_sz]; 
  
  /* Functions for MAC Timers */
  srslte::timers  timers_db;
  void            setup_timers();
  void            timeAlignmentTimerExpire();
  
  // pointer to MAC PCAP object
  srslte::mac_pcap* pcap;
  bool signals_pregenerated;
  bool is_first_ul_grant;


  mac_metrics_t metrics; 


  /* Class to run upper-layer timers with normal priority */
  class upper_timers : public thread {
  public: 
    upper_timers() : timers_db(MAC_NOF_UPPER_TIMERS),ttisync(10240) {start();}
    void tti_clock();
    void stop();
    void reset();
    srslte::timers::timer* get(uint32_t timer_id);
    uint32_t get_unique_id();
  private:
    void run_thread();
    srslte::timers  timers_db;
    srslte::tti_sync_cv     ttisync;
    bool running; 
  };
  upper_timers   upper_timers_thread; 



  /* Class to process MAC PDUs from DEMUX unit */
  class pdu_process : public thread {
  public: 
    pdu_process(demux *demux_unit);
    void notify();
    void stop();
  private:
    void run_thread();
    bool running; 
    bool have_data; 
    pthread_mutex_t mutex;
    pthread_cond_t  cvar;
    demux* demux_unit;
  };
  pdu_process pdu_process_thread;
};

} // namespace srsue

#endif // MAC_H
