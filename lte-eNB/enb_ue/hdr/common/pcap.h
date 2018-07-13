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

#ifndef UEPCAP_H
#define UEPCAP_H

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAC_LTE_DLT 147


/* This structure gets written to the start of the file */
typedef struct pcap_hdr_s {
        unsigned int   magic_number;   /* magic number */
        unsigned short version_major;  /* major version number */
        unsigned short version_minor;  /* minor version number */
        unsigned int   thiszone;       /* GMT to local correction */
        unsigned int   sigfigs;        /* accuracy of timestamps */
        unsigned int   snaplen;        /* max length of captured packets, in octets */
        unsigned int   network;        /* data link type */
} pcap_hdr_t;

/* This structure precedes each packet */
typedef struct pcaprec_hdr_s {
        unsigned int   ts_sec;         /* timestamp seconds */
        unsigned int   ts_usec;        /* timestamp microseconds */
        unsigned int   incl_len;       /* number of octets of packet saved in file */
        unsigned int   orig_len;       /* actual length of packet */
} pcaprec_hdr_t;


/* radioType */
#define FDD_RADIO 1
#define TDD_RADIO 2

/* Direction */
#define DIRECTION_UPLINK   0
#define DIRECTION_DOWNLINK 1

/* rntiType */
#define NO_RNTI  0  /* Used for BCH-BCH */
#define P_RNTI   1
#define RA_RNTI  2
#define C_RNTI   3
#define SI_RNTI  4
#define SPS_RNTI 5
#define M_RNTI   6

#define MAC_LTE_START_STRING "mac-lte"

#define MAC_LTE_RNTI_TAG            0x02
/* 2 bytes, network order */

#define MAC_LTE_UEID_TAG            0x03
/* 2 bytes, network order */

#define MAC_LTE_SUBFRAME_TAG        0x04
/* 2 bytes, network order */

#define MAC_LTE_PREDFINED_DATA_TAG  0x05
/* 1 byte */

#define MAC_LTE_RETX_TAG            0x06
/* 1 byte */

#define MAC_LTE_CRC_STATUS_TAG      0x07
/* 1 byte */

/* MAC PDU. Following this tag comes the actual MAC PDU (there is no length, the PDU
   continues until the end of the frame) */
#define MAC_LTE_PAYLOAD_TAG 0x01


/* Context information for every MAC PDU that will be logged */
typedef struct MAC_Context_Info_t {
    unsigned short radioType;
    unsigned char  direction;
    unsigned char  rntiType;
    unsigned short rnti;
    unsigned short ueid;
    unsigned char  isRetx;
    unsigned char  crcStatusOK;

    unsigned short sysFrameNumber;
    unsigned short subFrameNumber;

} MAC_Context_Info_t;




/**************************************************************************/
/* API functions for opening/writing/closing MAC-LTE PCAP files           */

/* Open the file and write file header */
inline FILE *MAC_LTE_PCAP_Open(const char *fileName)
{
    pcap_hdr_t file_header =
    {
        0xa1b2c3d4,   /* magic number */
        2, 4,         /* version number is 2.4 */
        0,            /* timezone */
        0,            /* sigfigs - apparently all tools do this */
        65535,        /* snaplen - this should be long enough */
        MAC_LTE_DLT   /* Data Link Type (DLT).  Set as unused value 147 for now */
    };

    FILE *fd = fopen(fileName, "w");
    if (fd == NULL) {
        printf("Failed to open file \"%s\" for writing\n", fileName);
        return NULL;
    }

    /* Write the file header */
    fwrite(&file_header, sizeof(pcap_hdr_t), 1, fd);

    return fd;
}

/* Write an individual PDU (PCAP packet header + mac-context + mac-pdu) */
inline int MAC_LTE_PCAP_WritePDU(FILE *fd, MAC_Context_Info_t *context,
                          const unsigned char *PDU, unsigned int length)
{
    pcaprec_hdr_t packet_header;
    char context_header[256];
    int offset = 0;
    unsigned short tmp16;

    /* Can't write if file wasn't successfully opened */
    if (fd == NULL) {
        printf("Error: Can't write to empty file handle\n");
        return 0;
    }

    /*****************************************************************/
    /* Context information (same as written by UDP heuristic clients */
    context_header[offset++] = context->radioType;
    context_header[offset++] = context->direction;
    context_header[offset++] = context->rntiType;

    /* RNTI */
    context_header[offset++] = MAC_LTE_RNTI_TAG;
    tmp16 = htons(context->rnti);
    memcpy(context_header+offset, &tmp16, 2);
    offset += 2;

    /* UEId */
    context_header[offset++] = MAC_LTE_UEID_TAG;
    tmp16 = htons(context->ueid);
    memcpy(context_header+offset, &tmp16, 2);
    offset += 2;

    /* Subframe number */
    context_header[offset++] = MAC_LTE_SUBFRAME_TAG;
    tmp16 = htons(context->subFrameNumber);
    memcpy(context_header+offset, &tmp16, 2);
    offset += 2;

    /* CRC Status */
    context_header[offset++] = MAC_LTE_CRC_STATUS_TAG;
    context_header[offset++] = context->crcStatusOK;

    /* Data tag immediately preceding PDU */
    context_header[offset++] = MAC_LTE_PAYLOAD_TAG;


    /****************************************************************/
    /* PCAP Header                                                  */
    struct timeval t;
    gettimeofday(&t, NULL);
    packet_header.ts_sec = t.tv_sec;
    packet_header.ts_usec = t.tv_usec;
    packet_header.incl_len = offset + length;
    packet_header.orig_len = offset + length;

    /***************************************************************/
    /* Now write everything to the file                            */
    fwrite(&packet_header, sizeof(pcaprec_hdr_t), 1, fd);
    fwrite(context_header, 1, offset, fd);
    fwrite(PDU, 1, length, fd);

    return 1;
}

/* Close the PCAP file */
inline void MAC_LTE_PCAP_Close(FILE *fd)
{
  if(fd)
    fclose(fd);
}

#endif /* UEPCAP_H */