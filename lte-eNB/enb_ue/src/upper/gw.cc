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


#include "upper/gw.h"

#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>


using namespace srslte;

namespace srsue{

gw::gw()
  :if_up(false)
{}

void gw::init(pdcp_interface_gw *pdcp_, rrc_interface_gw *rrc_, ue_interface *ue_, srslte::log *gw_log_)
{
  pool    = buffer_pool::get_instance();
  pdcp    = pdcp_;
  rrc     = rrc_;
  ue      = ue_;
  gw_log  = gw_log_;
  running = true;

  metrics_time = bpt::microsec_clock::local_time();
  dl_tput_bytes = 0;
  ul_tput_bytes = 0;
}

void gw::stop()
{
  if(running)
  {
    running = false;
    if(if_up)
    {
      thread_cancel();
      wait_thread_finish();
    }

    // TODO: tear down TUN device?
  }
}

void gw::get_metrics(gw_metrics_t &m)
{
  bpt::ptime now = bpt::microsec_clock::local_time();
  bpt::time_duration td = now - metrics_time;
  double secs = td.total_microseconds()/(double)1e6;
  m.dl_tput_mbps = (dl_tput_bytes*8/(double)1e6)/secs;
  m.ul_tput_mbps = (ul_tput_bytes*8/(double)1e6)/secs;
  gw_log->info("RX throughput: %4.6f Mbps. TX throughput: %4.6f Mbps.\n",
               m.dl_tput_mbps, m.ul_tput_mbps);
  metrics_time = now;
  dl_tput_bytes = 0;
  ul_tput_bytes = 0;
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void gw::write_pdu(uint32_t lcid, srslte::byte_buffer_t *pdu)
{
  gw_log->info_hex(pdu->msg, pdu->N_bytes, "RX PDU");
  gw_log->info("RX PDU. Stack latency: %ld us\n", pdu->get_latency_us());
  dl_tput_bytes += pdu->N_bytes;
  if(!if_up)
  {
    gw_log->warning("TUN/TAP not up - dropping gw RX message\n");
  }else{
    int n = write(tun_fd, pdu->msg, pdu->N_bytes); 
    if(pdu->N_bytes != n)
    {
      gw_log->warning("DL TUN/TAP write failure\n");
    } 
  }
  pool->deallocate(pdu);
}

/*******************************************************************************
  NAS interface
*******************************************************************************/
srslte::error_t gw::setup_if_addr(uint32_t ip_addr, char *err_str)
{
  if(!if_up)
  {
      if(init_if(err_str))
      {
        gw_log->error("init_if failed\n");
        return(ERROR_CANT_START);
      }
  }

  // Setup the IP address
  sock                                                   = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family                                 = AF_INET;
  ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = htonl(ip_addr);
  if(0 > ioctl(sock, SIOCSIFADDR, &ifr))
  {
      err_str = strerror(errno);
      gw_log->debug("Failed to set socket address: %s\n", err_str);
      close(tun_fd);
      return(ERROR_CANT_START);
  }
  ifr.ifr_netmask.sa_family                                 = AF_INET;
  ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr = inet_addr("255.255.255.0");
  if(0 > ioctl(sock, SIOCSIFNETMASK, &ifr))
  {
      err_str = strerror(errno);
      gw_log->debug("Failed to set socket netmask: %s\n", err_str);
      close(tun_fd);
      return(ERROR_CANT_START);
  }

  // Setup a thread to receive packets from the TUN device
  start(GW_THREAD_PRIO);

  return(ERROR_NONE);
}

srslte::error_t gw::init_if(char *err_str)
{
    if(if_up)
    {
      return(ERROR_ALREADY_STARTED);
    }

    char dev[IFNAMSIZ] = "tun_srsue";

    // Construct the TUN device
    tun_fd = open("/dev/net/tun", O_RDWR);
    gw_log->info("TUN file descriptor = %d\n", tun_fd);
    if(0 > tun_fd)
    {
        err_str = strerror(errno);
        gw_log->debug("Failed to open TUN device: %s\n", err_str);
        return(ERROR_CANT_START);
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_ifrn.ifrn_name, dev, IFNAMSIZ);
    if(0 > ioctl(tun_fd, TUNSETIFF, &ifr))
    {
        err_str = strerror(errno);
        gw_log->debug("Failed to set TUN device name: %s\n", err_str);
        close(tun_fd);
        return(ERROR_CANT_START);
    }

    // Bring up the interface
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(0 > ioctl(sock, SIOCGIFFLAGS, &ifr))
    {
        err_str = strerror(errno);
        gw_log->debug("Failed to bring up socket: %s\n", err_str);
        close(tun_fd);
        return(ERROR_CANT_START);
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if(0 > ioctl(sock, SIOCSIFFLAGS, &ifr))
    {
        err_str = strerror(errno);
        gw_log->debug("Failed to set socket flags: %s\n", err_str);
        close(tun_fd);
        return(ERROR_CANT_START);
    }

    if_up = true;

    return(ERROR_NONE);
}

/********************/
/*    GW Receive    */
/********************/
void gw::run_thread()
{
    struct iphdr   *ip_pkt;
    uint32          idx = 0;
    int32           N_bytes;
    byte_buffer_t  *pdu = pool->allocate();

    gw_log->info("GW IP packet receiver thread running\n");

    while(running)
    {
      if (pdu) {
        if (SRSUE_MAX_BUFFER_SIZE_BYTES-SRSUE_BUFFER_HEADER_OFFSET > idx) {
          N_bytes = read(tun_fd, &pdu->msg[idx], SRSUE_MAX_BUFFER_SIZE_BYTES-SRSUE_BUFFER_HEADER_OFFSET - idx);
        } else {
          gw_log->error("GW pdu buffer full - gw receive thread exiting.\n");
          gw_log->console("GW pdu buffer full - gw receive thread exiting.\n");
          break; 
        }
        gw_log->debug("Read %d bytes from TUN fd=%d, idx=%d\n", N_bytes, tun_fd, idx);
        if(N_bytes > 0)
        {
          pdu->N_bytes = idx + N_bytes;
          ip_pkt       = (struct iphdr*)pdu->msg;

          // Warning: Accept only IPv4 packets
          if (ip_pkt->version == 4) {
            // Check if entire packet was received
            if(ntohs(ip_pkt->tot_len) == pdu->N_bytes)
            {
              gw_log->info_hex(pdu->msg, pdu->N_bytes, "TX PDU");

              while(running && (!rrc->rrc_connected() || !rrc->have_drb())) {
                rrc->rrc_connect();
                usleep(1000);
              }
              
              if (!running) {
                break;
              }
              
              // Send PDU directly to PDCP
              pdu->timestamp = bpt::microsec_clock::local_time();
              ul_tput_bytes += pdu->N_bytes;
              pdcp->write_sdu(RB_ID_DRB1, pdu);
              
              pdu = pool->allocate();
              idx = 0;
            }else{
              idx += N_bytes;
            }            
          } 
        }else{
          gw_log->error("Failed to read from TUN interface - gw receive thread exiting.\n");
          gw_log->console("Failed to read from TUN interface - gw receive thread exiting.\n");
          break;
        }
      } else {
        gw_log->error("Could not allocate a PDU\n");
        gw_log->console("GW could not allocate a PDU\n");
        break;        
      }
    }

    gw_log->info("GW IP receiver thread exiting.\n");
}

} // namespace srsue
