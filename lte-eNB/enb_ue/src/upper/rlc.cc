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
 * MERCHANTABILITY or FITNESS FOR A PARTICRXAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */


#include "upper/rlc.h"
#include "upper/rlc_tm.h"
#include "upper/rlc_um.h"
#include "upper/rlc_am.h"

using namespace srslte;

namespace srsue{

rlc::rlc()
{
  pool = buffer_pool::get_instance();
}

void rlc::init(pdcp_interface_rlc *pdcp_,
               rrc_interface_rlc  *rrc_,
               ue_interface       *ue_,
               srslte::log        *rlc_log_, 
               mac_interface_timers *mac_timers_)
{
  pdcp    = pdcp_;
  rrc     = rrc_;
  ue      = ue_;
  rlc_log = rlc_log_;
  mac_timers = mac_timers_;

  metrics_time = bpt::microsec_clock::local_time();
  reset_metrics(); 

  rlc_array[0].init(RLC_MODE_TM, rlc_log, RB_ID_SRB0, pdcp, rrc, mac_timers); // SRB0
}

void rlc::reset_metrics() 
{
  bzero(dl_tput_bytes, sizeof(long)*SRSUE_N_RADIO_BEARERS);
  bzero(ul_tput_bytes, sizeof(long)*SRSUE_N_RADIO_BEARERS);
}

void rlc::stop()
{
  reset();
}

void rlc::get_metrics(rlc_metrics_t &m)
{
  bpt::ptime now = bpt::microsec_clock::local_time();
  bpt::time_duration td = now - metrics_time;
  double secs = td.total_microseconds()/(double)1e6;
  
  m.dl_tput_mbps = 0; 
  m.ul_tput_mbps = 0; 
  for (int i=0;i<SRSUE_N_RADIO_BEARERS;i++) {
    m.dl_tput_mbps += (dl_tput_bytes[i]*8/(double)1e6)/secs;
    m.ul_tput_mbps += (ul_tput_bytes[i]*8/(double)1e6)/secs;    
    if(rlc_array[i].active()) {
      rlc_log->info("LCID=%d, TX throughput: %4.6f Mbps. RX throughput: %4.6f Mbps.\n",
                    i,
                    (dl_tput_bytes[i]*8/(double)1e6)/secs,
                    (ul_tput_bytes[i]*8/(double)1e6)/secs);
    }
  }

  metrics_time = now;
  reset_metrics();
}

void rlc::reset()
{
  for(uint32_t i=0; i<SRSUE_N_RADIO_BEARERS; i++) {
    if(rlc_array[i].active())
      rlc_array[i].reset();
  }

  rlc_array[0].init(RLC_MODE_TM, rlc_log, RB_ID_SRB0, pdcp, rrc, mac_timers); // SRB0
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void rlc::write_sdu(uint32_t lcid, byte_buffer_t *sdu)
{
  if(valid_lcid(lcid)) {
    rlc_array[lcid].write_sdu(sdu);
  }
}

/*******************************************************************************
  MAC interface
*******************************************************************************/
uint32_t rlc::get_buffer_state(uint32_t lcid)
{
  if(valid_lcid(lcid)) {
    return rlc_array[lcid].get_buffer_state();
  } else {
    return 0;
  }
}

uint32_t rlc::get_total_buffer_state(uint32_t lcid)
{
  if(valid_lcid(lcid)) {
    return rlc_array[lcid].get_total_buffer_state();
  } else {
    return 0;
  }
}

int rlc::read_pdu(uint32_t lcid, uint8_t *payload, uint32_t nof_bytes)
{printf("HERE RLC\n");
  if(valid_lcid(lcid)) {
    ul_tput_bytes[lcid] += nof_bytes;
    return rlc_array[lcid].read_pdu(payload, nof_bytes);
  }
  return 0;
}

void rlc::write_pdu(uint32_t lcid, uint8_t *payload, uint32_t nof_bytes)
{
  if(valid_lcid(lcid)) {
    dl_tput_bytes[lcid] += nof_bytes;
    rlc_array[lcid].write_pdu(payload, nof_bytes);
  }
}

void rlc::write_pdu_bcch_bch(uint8_t *payload, uint32_t nof_bytes)
{
  rlc_log->info_hex(payload, nof_bytes, "BCCH BCH message received.");
  dl_tput_bytes[0] += nof_bytes;
  byte_buffer_t *buf = pool->allocate();
  memcpy(buf->msg, payload, nof_bytes);
  buf->N_bytes = nof_bytes;
  buf->timestamp = bpt::microsec_clock::local_time();
  pdcp->write_pdu_bcch_bch(buf);
}

void rlc::write_pdu_bcch_dlsch(uint8_t *payload, uint32_t nof_bytes)
{
  rlc_log->info_hex(payload, nof_bytes, "BCCH TXSCH message received.");
  dl_tput_bytes[0] += nof_bytes;
  byte_buffer_t *buf = pool->allocate();
  memcpy(buf->msg, payload, nof_bytes);
  buf->N_bytes = nof_bytes;
  buf->timestamp = bpt::microsec_clock::local_time();
  pdcp->write_pdu_bcch_dlsch(buf);
}

void rlc::write_pdu_pcch(uint8_t *payload, uint32_t nof_bytes)
{
  rlc_log->info_hex(payload, nof_bytes, "PCCH message received.");
  dl_tput_bytes[0] += nof_bytes;
  byte_buffer_t *buf = pool->allocate();
  memcpy(buf->msg, payload, nof_bytes);
  buf->N_bytes = nof_bytes;
  buf->timestamp = bpt::microsec_clock::local_time();
  pdcp->write_pdu_pcch(buf);
}

/*******************************************************************************
  RRC interface
*******************************************************************************/
void rlc::add_bearer(uint32_t lcid)
{
  // No config provided - use defaults for lcid
  LIBLTE_RRC_RLC_CONFIG_STRUCT cnfg;
  if(RB_ID_SRB1 == lcid || RB_ID_SRB2 == lcid)
  {
    if (!rlc_array[lcid].active()) {
      cnfg.rlc_mode                     = LIBLTE_RRC_RLC_MODE_AM;
      cnfg.ul_am_rlc.t_poll_retx        = LIBLTE_RRC_T_POLL_RETRANSMIT_MS45;
      cnfg.ul_am_rlc.poll_pdu           = LIBLTE_RRC_POLL_PDU_INFINITY;
      cnfg.ul_am_rlc.poll_byte          = LIBLTE_RRC_POLL_BYTE_INFINITY;
      cnfg.ul_am_rlc.max_retx_thresh    = LIBLTE_RRC_MAX_RETX_THRESHOLD_T4;
      cnfg.dl_am_rlc.t_reordering       = LIBLTE_RRC_T_REORDERING_MS35;
      cnfg.dl_am_rlc.t_status_prohibit  = LIBLTE_RRC_T_STATUS_PROHIBIT_MS0;
      add_bearer(lcid, &cnfg);
    } else {
      rlc_log->warning("Bearer %s already configured. Reconfiguration not supported\n", rb_id_text[lcid]);
    }
  }else{
    rlc_log->error("Radio bearer %s does not support default RLC configuration.",
                   rb_id_text[lcid]);
  }
}

void rlc::add_bearer(uint32_t lcid, LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg)
{
  if(lcid < 0 || lcid >= SRSUE_N_RADIO_BEARERS) {
    rlc_log->error("Radio bearer id must be in [0:%d] - %d\n", SRSUE_N_RADIO_BEARERS, lcid);
    return;
  }
  
  
  if (!rlc_array[lcid].active()) {
    rlc_log->info("Adding radio bearer %s with mode %s\n",
                    rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode]);  
    switch(cnfg->rlc_mode)
    {
    case LIBLTE_RRC_RLC_MODE_AM:
      rlc_array[lcid].init(RLC_MODE_AM, rlc_log, lcid, pdcp, rrc, mac_timers);
      break;
    case LIBLTE_RRC_RLC_MODE_UM_BI:
      rlc_array[lcid].init(RLC_MODE_UM, rlc_log, lcid, pdcp, rrc, mac_timers);
      break;
    case LIBLTE_RRC_RLC_MODE_UM_UNI_DL:
      rlc_array[lcid].init(RLC_MODE_UM, rlc_log, lcid, pdcp, rrc, mac_timers);
      break;
    case LIBLTE_RRC_RLC_MODE_UM_UNI_UL:
      rlc_array[lcid].init(RLC_MODE_UM, rlc_log, lcid, pdcp, rrc, mac_timers);
      break;
    default:
      rlc_log->error("Cannot add RLC entity - invalid mode\n");
      return;
    }
  } else {
    rlc_log->warning("Bearer %s already created.\n", rb_id_text[lcid]);
  }
  rlc_array[lcid].configure(cnfg);    

}

/*******************************************************************************
  Helpers
*******************************************************************************/
bool rlc::valid_lcid(uint32_t lcid)
{
  if(lcid < 0 || lcid >= SRSUE_N_RADIO_BEARERS) {
    return false;
  }
  if(!rlc_array[lcid].active()) {
    return false;
  }
  return true;
}


} // namespace srsue
