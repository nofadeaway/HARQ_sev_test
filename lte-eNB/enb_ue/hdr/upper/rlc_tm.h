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

#ifndef RLC_TM_H
#define RLC_TM_H

#include "common/buffer_pool.h"
#include "common/log.h"
#include "common/common.h"
#include "common/interfaces.h"
#include "common/msg_queue.h"
#include "upper/rlc_common.h"

namespace srsue {

class rlc_tm
    :public rlc_common
{
public:
  rlc_tm();
  void init(srslte::log          *rlc_entity_log_,
            uint32_t              lcid_,
            pdcp_interface_rlc   *pdcp_,
            rrc_interface_rlc    *rrc_,
            srslte::mac_interface_timers *mac_timers);
  void configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg);
  void reset();
  void empty_queue(); 

  rlc_mode_t    get_mode();
  uint32_t      get_bearer();

  // PDCP interface
  void write_sdu(srslte::byte_buffer_t *sdu);

  // MAC interface
  uint32_t get_buffer_state();
  uint32_t get_total_buffer_state();
  int      read_pdu(uint8_t *payload, uint32_t nof_bytes);
  void     write_pdu(uint8_t *payload, uint32_t nof_bytes);

private:

  srslte::buffer_pool        *pool;
  srslte::log        *log;
  uint32_t            lcid;
  pdcp_interface_rlc *pdcp;
  rrc_interface_rlc  *rrc;

  // Thread-safe queues for MAC messages
  srslte::msg_queue    ul_queue;
};

} // namespace srsue


#endif // RLC_TM_H
