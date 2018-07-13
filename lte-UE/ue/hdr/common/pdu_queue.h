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

#ifndef PDUPROC_H
#define PDUPROC_H

#include "common/log.h"
#include "common/qbuff.h"
#include "common/timers.h"
#include "common/pdu.h"

/* Logical Channel Demultiplexing and MAC CE dissassemble */   


namespace srslte {

class pdu_queue
{
public:
  class process_callback
  {
    public: 
      virtual void process_pdu(uint8_t *buff, uint32_t len) = 0;
      //process_callback();              //自己添加的5.22
      //void process_pdu(uint8_t *buff, uint32_t len);  //自己添加的5.22
  };

  pdu_queue();
  void init(process_callback *callback, log* log_h_);

  bool     process_pdus();
  uint8_t* request_buffer(uint32_t pid, uint32_t len);
  
  void     push_pdu(uint32_t pid, uint32_t nof_bytes);
    
  std::vector<qbuff> pdu_q;    //PDU buffer   FX:5.29添加

private:
  const static int NOF_HARQ_PID    = 8; 
  const static int MAX_PDU_LEN     = 150*1024/8; // ~ 150 Mbps  
  const static int NOF_BUFFER_PDUS = 64; // Number of PDU buffers per HARQ pid
        
  //std::vector<qbuff> pdu_q;    //PDU buffer     //FX:5.29 因为别的地方要调用，将其改为public
  process_callback *callback; 
  
  log       *log_h;
  bool initiated; 
};

} // namespace srslte

#endif // PDUPROC_H



