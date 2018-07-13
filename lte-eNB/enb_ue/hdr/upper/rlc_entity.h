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

#ifndef RLC_ENTITY_H
#define RLC_ENTITY_H

#include "common/log.h"
#include "common/common.h"
#include "common/interfaces.h"
#include "upper/rlc_common.h"
#include "upper/rlc_tm.h"
#include "upper/rlc_um.h"
#include "upper/rlc_am.h"

namespace srsue {



/****************************************************************************
 * RLC Entity
 * Common container for all RLC entities
 ***************************************************************************/
class rlc_entity
{
public:
  rlc_entity();
  void init(rlc_mode_t            mode,
            srslte::log          *rlc_entity_log_,
            uint32_t              lcid_,
            pdcp_interface_rlc   *pdcp_,
            rrc_interface_rlc    *rrc_,
            srslte::mac_interface_timers *mac_timers_);

  void configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg);
  void reset();
  bool active();

  rlc_mode_t    get_mode();
  uint32_t      get_bearer();

  // PDCP interface
  void write_sdu(byte_buffer_t *sdu);

  // MAC interface
  uint32_t get_buffer_state();
  uint32_t get_total_buffer_state();
  int      read_pdu(uint8_t *payload, uint32_t nof_bytes);
  void     write_pdu(uint8_t *payload, uint32_t nof_bytes);

private:
  rlc_tm tm;
  rlc_um um;
  rlc_am am;

  rlc_common *rlc;
};

} // namespace srsue


#endif // RLC_ENTITY_H
