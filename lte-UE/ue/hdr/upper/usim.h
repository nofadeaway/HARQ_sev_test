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

#ifndef USIM_H
#define USIM_H

#include <string>
#include "common/log.h"
#include "common/common.h"
#include "common/interfaces.h"
#include "common/security.h"

namespace srsue {

typedef enum{
  auth_algo_milenage = 0,
  auth_algo_xor,
}auth_algo_t;

typedef struct{
  std::string algo;
  std::string op;
  std::string amf;
  std::string imsi;
  std::string imei;
  std::string k;
}usim_args_t;

class usim
    :public usim_interface_nas
    ,public usim_interface_rrc
{
public:
  usim();
  void init(usim_args_t *args, srslte::log *usim_log_);
  void stop();

  // NAS interface
  void get_imsi_vec(uint8_t* imsi_, uint32_t n);
  void get_imei_vec(uint8_t* imei_, uint32_t n);

  void generate_authentication_response(uint8_t  *rand,
                                        uint8_t  *autn_enb,
                                        uint16_t  mcc,
                                        uint16_t  mnc,
                                        bool     *net_valid,
                                        uint8_t  *res);

  void generate_nas_keys(uint8_t *k_nas_enc,
                         uint8_t *k_nas_int,
                         CIPHERING_ALGORITHM_ID_ENUM cipher_algo,
                         INTEGRITY_ALGORITHM_ID_ENUM integ_algo);

  // RRC interface
  void generate_as_keys(uint32_t count_ul,
                        uint8_t *k_rrc_enc,
                        uint8_t *k_rrc_int,
                        uint8_t *k_up_enc,
                        uint8_t *k_up_int,
                        CIPHERING_ALGORITHM_ID_ENUM cipher_algo,
                        INTEGRITY_ALGORITHM_ID_ENUM integ_algo);


private:
  void gen_auth_res_milenage( uint8_t  *rand,
                              uint8_t  *autn_enb,
                              uint16_t  mcc,
                              uint16_t  mnc,
                              bool     *net_valid,
                              uint8_t  *res);
  void gen_auth_res_xor(      uint8_t  *rand,
                              uint8_t  *autn_enb,
                              uint16_t  mcc,
                              uint16_t  mnc,
                              bool     *net_valid,
                              uint8_t  *res);
  void str_to_hex(std::string str, uint8_t *hex);

  srslte::log *usim_log;

  // User data
  auth_algo_t auth_algo;
  uint8_t     amf[2];  // 3GPP 33.102 v10.0.0 Annex H
  uint8_t     op[16];
  uint64_t    imsi;
  uint64_t    imei;
  uint8_t     k[16];

  // Security variables
  uint8_t     rand[16];
  uint8_t     ck[16];
  uint8_t     ik[16];
  uint8_t     ak[6];
  uint8_t     mac[8];
  uint8_t     autn[16];
  uint8_t     k_asme[32];
  uint8_t     k_enb[32];

};

} // namespace srsue


#endif // USIM_H
