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


#define Error(fmt, ...)   log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#include "common/pdu_queue.h"


namespace srslte {
    
pdu_queue::pdu_queue() : pdu_q(NOF_HARQ_PID)
{
  callback = NULL; 
}

void pdu_queue::init(process_callback *callback_, log* log_h_)
{
  callback  = callback_;
  log_h     = log_h_; 
  for (int i=0;i<NOF_HARQ_PID;i++) {
    pdu_q[i].init(NOF_BUFFER_PDUS, MAX_PDU_LEN);
  }
  initiated = true; 
}

uint8_t* pdu_queue::request_buffer(uint32_t pid, uint32_t len)
{  
  if (!initiated) {
    return NULL; 
  }

  uint8_t *buff = NULL; 

  if (pid < NOF_HARQ_PID) {
    if (len < MAX_PDU_LEN) {
      if (pdu_q[pid].pending_msgs() > 0.75*pdu_q[pid].max_msgs()) {
        log_h->console("Warning TX buffer HARQ PID=%d: Occupation is %.1f%% \n", 
                      pid, (float) 100*pdu_q[pid].pending_msgs()/pdu_q[pid].max_msgs());
      }
      buff = (uint8_t*) pdu_q[pid].request();
      if (!buff) {
        Error("Error Buffer full for HARQ PID=%d\n", pid);
        log_h->error("Error Buffer full for HARQ PID=%d\n", pid);
        return NULL;
      }      
    } else {
      Error("Requested too large buffer for PID=%d. Requested %d bytes, max length %d bytes\n", 
            pid, len, MAX_PDU_LEN);
    }
  } else {
    Error("Requested buffer for invalid PID=%d\n", pid);
  }
  return buff; 
}

/* Demultiplexing of logical channels and dissassemble of MAC CE 
 * This function enqueues the packet and returns quicly because ACK 
 * deadline is important here. 
 */ 
void pdu_queue::push_pdu(uint32_t pid, uint32_t nof_bytes)
{
  if (!initiated) {
    return; 
  }
  
  if (pid < NOF_HARQ_PID) {    
    if (nof_bytes > 0) {
      if (!pdu_q[pid].push(nof_bytes)) {
        Warning("Full queue %d when pushing MAC PDU %d bytes\n", pid, nof_bytes);
      }
      //callback->process_pdu((uint8_t*) pdu_q[pid].request(), nof_bytes);
    } else {
      Warning("Trying to push PDU with payload size zero\n");
    }
  } else {
    Error("Pushed buffer for invalid PID=%d\n", pid);
  }  
}

bool pdu_queue::process_pdus()
{
  if (!initiated) {
    return false; 
  }

  bool have_data = false; 
  for (int i=0;i<NOF_HARQ_PID;i++) {
    uint8_t *buff = NULL;
    uint32_t len  = 0; 
    uint32_t cnt  = 0; 
    do {
      buff = (uint8_t*) pdu_q[i].pop(&len);
      if (buff) {
        if (callback) {
          callback->process_pdu(buff, len);
        }
        pdu_q[i].release();
        cnt++;
        have_data = true;
      }
    } while(buff);
    if (cnt > 20) {
      log_h->console("Warning dispatched %d packets for PID=%d\n", cnt, i);
    }
  }
  return have_data; 
}

}
