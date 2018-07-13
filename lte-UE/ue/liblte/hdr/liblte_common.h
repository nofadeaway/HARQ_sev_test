/*******************************************************************************

    Copyright 2012-2014 Ben Wojtowicz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

    File: liblte_common.h

    Description: Contains all the common definitions for the LTE library.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2012    Ben Wojtowicz    Created file.
    07/21/2013    Ben Wojtowicz    Added a common message structure.
    06/15/2014    Ben Wojtowicz    Split LIBLTE_MSG_STRUCT into bit and byte
                                   aligned messages.
    08/03/2014    Ben Wojtowicz    Commonized value_2_bits and bits_2_value.
    11/29/2014    Ben Wojtowicz    Added liblte prefix to value_2_bits and
                                   bits_2_value.

*******************************************************************************/

#ifndef __LIBLTE_COMMON_H__
#define __LIBLTE_COMMON_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"
#include <string.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// FIXME: This was chosen arbitrarily
#define LIBLTE_MAX_MSG_SIZE_BITS  102048
#define LIBLTE_MAX_MSG_SIZE_BYTES 12756
#define LIBLTE_MSG_HEADER_OFFSET  1024

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LIBLTE_SUCCESS = 0,
    LIBLTE_ERROR_INVALID_INPUTS,
    LIBLTE_ERROR_DECODE_FAIL,
    LIBLTE_ERROR_INVALID_CRC,
}LIBLTE_ERROR_ENUM;

typedef struct{
    bool   data_valid;
    bool   data;
}LIBLTE_BOOL_MSG_STRUCT;

typedef struct{
    uint32 N_bits;
    uint8  msg[LIBLTE_MAX_MSG_SIZE_BITS];
}LIBLTE_SIMPLE_BIT_MSG_STRUCT;

typedef struct{
    uint32 N_bytes;
    uint8  msg[LIBLTE_MAX_MSG_SIZE_BYTES];
}LIBLTE_SIMPLE_BYTE_MSG_STRUCT;


struct LIBLTE_BYTE_MSG_STRUCT{
    uint32  N_bytes;
    uint8   buffer[LIBLTE_MAX_MSG_SIZE_BYTES];
    uint8  *msg;

    LIBLTE_BYTE_MSG_STRUCT():N_bytes(0)
    {
      msg = &buffer[LIBLTE_MSG_HEADER_OFFSET];
    }
    LIBLTE_BYTE_MSG_STRUCT(const LIBLTE_BYTE_MSG_STRUCT& buf)
    {
      N_bytes = buf.N_bytes;
      memcpy(msg, buf.msg, N_bytes);
    }
    LIBLTE_BYTE_MSG_STRUCT & operator= (const LIBLTE_BYTE_MSG_STRUCT & buf)
    {
      N_bytes = buf.N_bytes;
      memcpy(msg, buf.msg, N_bytes);
    }
    uint32 get_headroom()
    {
      return msg-buffer;
    }
};

struct LIBLTE_BIT_MSG_STRUCT{
    uint32  N_bits;
    uint8   buffer[LIBLTE_MAX_MSG_SIZE_BITS];
    uint8  *msg;

    LIBLTE_BIT_MSG_STRUCT():N_bits(0)
    {
      msg = &buffer[LIBLTE_MSG_HEADER_OFFSET];
    }
    LIBLTE_BIT_MSG_STRUCT(const LIBLTE_BIT_MSG_STRUCT& buf){
      N_bits = buf.N_bits;
      memcpy(msg, buf.msg, N_bits);
    }
    LIBLTE_BIT_MSG_STRUCT & operator= (const LIBLTE_BIT_MSG_STRUCT & buf){
      N_bits = buf.N_bits;
      memcpy(msg, buf.msg, N_bits);
    }
    uint32 get_headroom()
    {
      return msg-buffer;
    }
};


/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Name: liblte_value_2_bits

    Description: Converts a value to a bit string
*********************************************************************/
void liblte_value_2_bits(uint32   value,
                         uint8  **bits,
                         uint32   N_bits);

/*********************************************************************
    Name: liblte_bits_2_value

    Description: Converts a bit string to a value
*********************************************************************/
uint32 liblte_bits_2_value(uint8  **bits,
                           uint32   N_bits);

/*********************************************************************
    Name: pack

    Description: Pack a bit array into a byte array
*********************************************************************/
void pack(LIBLTE_BIT_MSG_STRUCT  *bits,
          LIBLTE_BYTE_MSG_STRUCT *bytes);

/*********************************************************************
    Name: unpack

    Description: Unpack a byte array into a bit array
*********************************************************************/
void unpack(LIBLTE_BYTE_MSG_STRUCT *bytes,
            LIBLTE_BIT_MSG_STRUCT  *bits);

#endif /* __LIBLTE_COMMON_H__ */
