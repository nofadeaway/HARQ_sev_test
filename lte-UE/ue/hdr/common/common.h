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

#ifndef COMMON_H
#define COMMON_H

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include <stdint.h>
#include <string.h>
#include <boost/date_time/posix_time/posix_time.hpp>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define SRSUE_UE_CATEGORY     4

#define SRSUE_N_SRB           3
#define SRSUE_N_DRB           8
#define SRSUE_N_RADIO_BEARERS 11

// Cat 3 UE - Max number of DL-SCH transport block bits received within a TTI
// 3GPP 36.306 Table 4.1.1
#define SRSUE_MAX_BUFFER_SIZE_BITS  102048
#define SRSUE_MAX_BUFFER_SIZE_BYTES 12756
#define SRSUE_BUFFER_HEADER_OFFSET  1024

namespace bpt = boost::posix_time;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

namespace srslte {

typedef enum{
  ERROR_NONE = 0,
  ERROR_INVALID_PARAMS,
  ERROR_INVALID_COMMAND,
  ERROR_OUT_OF_BOUNDS,
  ERROR_CANT_START,
  ERROR_ALREADY_STARTED,
  ERROR_N_ITEMS,
}error_t;
static const char error_text[ERROR_N_ITEMS][20] = { "None",
                                                    "Invalid parameters",
                                                    "Invalid command",
                                                    "Out of bounds",
                                                    "Can't start",
                                                    "Already started"};

typedef enum{
  RB_ID_SRB0 = 0,
  RB_ID_SRB1,
  RB_ID_SRB2,
  RB_ID_DRB1,
  RB_ID_DRB2,
  RB_ID_DRB3,
  RB_ID_DRB4,
  RB_ID_DRB5,
  RB_ID_DRB6,
  RB_ID_DRB7,
  RB_ID_DRB8,
  RB_ID_N_ITEMS,
}rb_id_t;
static const char rb_id_text[RB_ID_N_ITEMS][20] = { "SRB0",
                                                    "SRB1",
                                                    "SRB2",
                                                    "DRB1",
                                                    "DRB2",
                                                    "DRB3",
                                                    "DRB4",
                                                    "DRB5",
                                                    "DRB6",
                                                    "DRB7",
                                                    "DRB8"};

/******************************************************************************
 * Byte and Bit buffers
 *
 * Generic buffers with headroom to accommodate packet headers and custom
 * copy constructors & assignment operators for quick copying. Byte buffer
 * holds a next pointer to support linked lists.
 *****************************************************************************/
class byte_buffer_t{
public:
    uint32_t    N_bytes;
    uint8_t     buffer[SRSUE_MAX_BUFFER_SIZE_BYTES];
    uint8_t    *msg;
    bpt::ptime  timestamp;
    uint32_t     opt, opt2; 

    byte_buffer_t():N_bytes(0)
    {
      msg  = &buffer[SRSUE_BUFFER_HEADER_OFFSET];
      next = NULL; 
      opt  = 0; 
      opt2 = 0; 
    }
    byte_buffer_t(const byte_buffer_t& buf)
    {
      N_bytes = buf.N_bytes;
      memcpy(msg, buf.msg, N_bytes);
    }
    byte_buffer_t & operator= (const byte_buffer_t & buf)
    {
      N_bytes = buf.N_bytes;
      memcpy(msg, buf.msg, N_bytes);
    }
    void reset()
    {
      msg       = &buffer[SRSUE_BUFFER_HEADER_OFFSET];
      N_bytes   = 0;
      timestamp = bpt::not_a_date_time;
    }
    uint32_t get_headroom()
    {
      return msg-buffer;
    }
    long get_latency_us()
    {
      if(timestamp.is_not_a_date_time())
        return 0;
      bpt::time_duration td = bpt::microsec_clock::local_time() - timestamp;
      return td.total_microseconds();
    }

    // Linked list support
    byte_buffer_t*  get_next() { return next; }
    void set_next(byte_buffer_t *b) { next = b; }
private:
    byte_buffer_t *next;
};

struct bit_buffer_t{
    uint32_t    N_bits;
    uint8_t     buffer[SRSUE_MAX_BUFFER_SIZE_BITS];
    uint8_t    *msg;
    bpt::ptime  timestamp;

    bit_buffer_t():N_bits(0)
    {
      msg = &buffer[SRSUE_BUFFER_HEADER_OFFSET];
    }
    bit_buffer_t(const bit_buffer_t& buf){
      N_bits = buf.N_bits;
      memcpy(msg, buf.msg, N_bits);
    }
    bit_buffer_t & operator= (const bit_buffer_t & buf){
      N_bits = buf.N_bits;
      memcpy(msg, buf.msg, N_bits);
    }
    void reset()
    {
      msg       = &buffer[SRSUE_BUFFER_HEADER_OFFSET];
      N_bits    = 0;
      timestamp = bpt::not_a_date_time;
    }
    uint32_t get_headroom()
    {
      return msg-buffer;
    }
    long get_latency_us()
    {
      if(timestamp.is_not_a_date_time())
        return 0;
      bpt::time_duration td = bpt::microsec_clock::local_time() - timestamp;
      return td.total_microseconds();
    }
};

} // namespace srsue

#endif // COMMON_H
