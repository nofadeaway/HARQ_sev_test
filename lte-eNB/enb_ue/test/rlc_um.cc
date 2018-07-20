#include "upper/rlc_um.h"
#include "../hdr/upper/rlc_um.h"
#include "FuncHead.h"

extern int tun_fd;
//extern pthread_mutex_t mut;//lock
#define RX_MOD_BASE(x) (x-vr_uh-rx_window_size)%rx_mod

using namespace srslte;

namespace srsue{

rlc_um::rlc_um() : tx_sdu_queue(16)
{
  tx_sdu = NULL;
  rx_sdu = NULL;
  pool = buffer_pool::get_instance();

  vt_us    = 0;
  vr_ur    = 0;
  vr_ux    = 0;
  vr_uh    = 0;
  
  vr_ur_in_rx_sdu = 0; 
  
  mac_timers = NULL; 

  pdu_lost = false;
}

void rlc_um::init(srslte::log          *log_,
                  uint32_t              lcid_,
                  pdcp_interface_rlc   *pdcp_,
                  rrc_interface_rlc    *rrc_,
                  srslte::mac_interface_timers *mac_timers_)
{
  log                   = log_;
  lcid                  = lcid_;
  pdcp                  = pdcp_;
  rrc                   = rrc_;
  mac_timers            = mac_timers_;
  reordering_timeout_id = mac_timers->get_unique_id();
}

void rlc_um::configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg)
{
  switch(cnfg->rlc_mode)
  {
  case LIBLTE_RRC_RLC_MODE_UM_BI:
    t_reordering        = liblte_rrc_t_reordering_num[cnfg->dl_um_bi_rlc.t_reordering];
    rx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->dl_um_bi_rlc.sn_field_len;
    rx_window_size      = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 16 : 512;
    rx_mod              = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 32 : 1024;
    tx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->ul_um_bi_rlc.sn_field_len;
    tx_mod              = (RLC_UMD_SN_SIZE_5_BITS == tx_sn_field_length) ? 32 : 1024;
    // log->info("%s configured in %s mode: "
    //           "t_reordering=%d ms, rx_sn_field_length=%u bits, tx_sn_field_length=%u bits\n",
    //           rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
    //           t_reordering,
    //           rlc_umd_sn_size_num[rx_sn_field_length],
    //           rlc_umd_sn_size_num[tx_sn_field_length]);
    break;
  case LIBLTE_RRC_RLC_MODE_UM_UNI_UL:
    tx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->ul_um_uni_rlc.sn_field_len;
    tx_mod              = (RLC_UMD_SN_SIZE_5_BITS == tx_sn_field_length) ? 32 : 1024;
    // log->info("%s configured in %s mode: tx_sn_field_length=%u bits\n",
    //           rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
    //           rlc_umd_sn_size_num[tx_sn_field_length]);
    break;
  case LIBLTE_RRC_RLC_MODE_UM_UNI_DL:
    t_reordering        = liblte_rrc_t_reordering_num[cnfg->dl_um_uni_rlc.t_reordering];
    rx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->dl_um_uni_rlc.sn_field_len;
    rx_window_size      = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 16 : 512;
    rx_mod              = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 32 : 1024;
    // log->info("%s configured in %s mode: "
    //           "t_reordering=%d ms, rx_sn_field_length=%u bits\n",
    //           rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
    //           liblte_rrc_t_reordering_num[t_reordering],
    //           rlc_umd_sn_size_num[rx_sn_field_length]);
    break;
  default:
    log->error("RLC configuration mode not recognized\n");
  }
}

void rlc_um::empty_queue() {
  // Drop all messages in TX SDU queue
  byte_buffer_t *buf;
  while(tx_sdu_queue.size() > 0) {
    tx_sdu_queue.read(&buf);
    pool->deallocate(buf);
  }
}

void rlc_um::reset()
{
  vt_us    = 0;
  vr_ur    = 0;
  vr_ux    = 0;
  vr_uh    = 0;
  pdu_lost = false;
  if(rx_sdu)
    rx_sdu->reset();
  if(tx_sdu)
    tx_sdu->reset();
  if(mac_timers)
    mac_timers->get(reordering_timeout_id)->stop();

  empty_queue();
  
  // Drop all messages in RX window
  std::map<uint32_t, rlc_umd_pdu_t>::iterator it;
  for(it = rx_window.begin(); it != rx_window.end(); it++) {
    pool->deallocate(it->second.buf);
  }
  rx_window.clear();
}

rlc_mode_t rlc_um::get_mode()
{
  return RLC_MODE_UM;
}

uint32_t rlc_um::get_bearer()
{
  return lcid;
}

uint32_t rlc_um::n_unread()
{
  return tx_sdu_queue.size();
}
/****************************************************************************
 * PDCP interface
 ***************************************************************************/

void rlc_um::write_sdu(byte_buffer_t *sdu)
{
  //log->info_hex(sdu->msg, sdu->N_bytes, "%s Tx SDU", rb_id_text[lcid]);
  tx_sdu_queue.write(sdu);
}

/****************************************************************************
 * MAC interface
 ***************************************************************************/

uint32_t rlc_um::get_buffer_state()
{
  // Bytes needed for tx SDUs
  uint32_t n_sdus  = tx_sdu_queue.size();
  uint32_t n_bytes = tx_sdu_queue.size_bytes();
  if(tx_sdu)
  {
    n_sdus++;
    n_bytes += tx_sdu->N_bytes;
  }

  // Room needed for header extensions? (integer rounding)
  if(n_sdus > 1)
    n_bytes += ((n_sdus-1)*1.5)+0.5;

  // Room needed for fixed header?
  if(n_bytes > 0)
    n_bytes += 2;

  return n_bytes;
}

uint32_t rlc_um::get_total_buffer_state()
{
  return get_buffer_state();
}

int rlc_um::read_pdu(uint8_t *payload, uint32_t nof_bytes)  //payload带回最终组装的rlc_pdu包，nof_bytes为MAC给RLC反馈的包长度，返回值len为实际组包的长度
{
  //log->debug("MAC opportunity - %d bytes\n", nof_bytes);
 // log->debug("NOW--HERE\n");
  return build_data_pdu(payload, nof_bytes);
}

void rlc_um::write_pdu(uint8_t *payload, uint32_t nof_bytes)//msg记录的是数据包的地址，len为待处理数据包的长度。调用该函数后，程序会将数据包（msg记录的数据）送到RLC解包程序
{
  boost::lock_guard<boost::mutex> lock(mutex);
  handle_data_pdu(payload, nof_bytes);
}

/****************************************************************************
 * Timeout callback interface
 ***************************************************************************/

void rlc_um::timer_expired(uint32_t timeout_id)
{
  if(reordering_timeout_id == timeout_id)
  {
    boost::lock_guard<boost::mutex> lock(mutex);

    // 36.322 v10 Section 5.1.2.2.4
    // log->info("%s reordering timeout expiry - updating vr_ur and reassembling\n",
    //            rb_id_text[lcid]);

    log->warning("Lost PDU SN: %d\n", vr_ur);
    pdu_lost = true;
    rx_sdu->reset();
    while(RX_MOD_BASE(vr_ur) < RX_MOD_BASE(vr_ux))
    {
      vr_ur = (vr_ur + 1)%rx_mod;
      log->debug("Entering Reassemble from timeout id=%d\n", timeout_id);
      reassemble_rx_sdus();
      log->debug("Finished reassemble from timeout id=%d\n", timeout_id);
    }
    mac_timers->get(reordering_timeout_id)->stop();
    if(RX_MOD_BASE(vr_uh) > RX_MOD_BASE(vr_ur))
    {
      mac_timers->get(reordering_timeout_id)->set(this, t_reordering);
      mac_timers->get(reordering_timeout_id)->run();
      vr_ux = vr_uh;
    }

    debug_state();
  }
}

bool rlc_um::reordering_timeout_running()
{
  return mac_timers->get(reordering_timeout_id)->is_running();
}

/****************************************************************************
 * Helpers
 ***************************************************************************/

int  rlc_um::build_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  if(!tx_sdu && tx_sdu_queue.size() == 0)
  {
    // log->info("No data available to be sent\n");
    return 0;
  }

  byte_buffer_t *pdu = pool->allocate();
  if(!pdu || pdu->N_bytes != 0)
  {
    log->error("Failed to allocate PDU buffer\n");
    return 0;
  }
  rlc_umd_pdu_header_t header;
  header.fi   = RLC_FI_FIELD_START_AND_END_ALIGNED;
  header.sn   = vt_us;
  header.N_li = 0;
  header.sn_size = tx_sn_field_length;

  uint32_t to_move   = 0;
  uint32_t last_li   = 0;
  uint8_t *pdu_ptr   = pdu->msg;

  int head_len  = rlc_um_packed_length(&header);
  int pdu_space = nof_bytes;

  if(pdu_space <= head_len)
  {
    log->warning("%s Cannot build a PDU - %d bytes available, %d bytes required for header\n",
                 rb_id_text[lcid], nof_bytes, head_len);
    return 0;
  }

  // Check for SDU segment
  if(tx_sdu)
  {
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    log->debug("%s adding remainder of SDU segment - %d bytes of %d remaining\n",
               rb_id_text[lcid], to_move, tx_sdu->N_bytes);
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      // log->info("%s Complete SDU scheduled for tx. Stack latency: %ld us\n",
      //           rb_id_text[lcid], tx_sdu->get_latency_us());
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
    header.fi |= RLC_FI_FIELD_NOT_START_ALIGNED; // First byte does not correspond to first byte of SDU
  }

  // Pull SDUs from queue
  while(pdu_space > head_len && tx_sdu_queue.size() > 0)
  {
    log->debug("pdu_space=%d, head_len=%d\n", pdu_space, head_len);
    if(last_li > 0)
      header.li[header.N_li++] = last_li;
    head_len = rlc_um_packed_length(&header);
    tx_sdu_queue.read(&tx_sdu);
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    log->debug("%s adding new SDU segment - %d bytes of %d remaining\n",
               rb_id_text[lcid], to_move, tx_sdu->N_bytes);
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      // log->info("%s Complete SDU scheduled for tx. Stack latency: %ld us\n",
      //           rb_id_text[lcid], tx_sdu->get_latency_us());
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
  }

  if(tx_sdu)
    header.fi |= RLC_FI_FIELD_NOT_END_ALIGNED; // Last byte does not correspond to last byte of SDU

  // Set SN
  header.sn = vt_us;
  vt_us = (vt_us + 1)%tx_mod;

  // Add header and TX
  log->debug("%s packing PDU with length %d\n", rb_id_text[lcid], pdu->N_bytes);
  rlc_um_write_data_pdu_header(&header, pdu);
  memcpy(payload, pdu->msg, pdu->N_bytes);
  uint32_t ret = pdu->N_bytes;
  log->debug("%s returning length %d\n", rb_id_text[lcid], pdu->N_bytes);
  pool->deallocate(pdu);

  debug_state();//此时的
  return ret;
}

//RLC实际解包程序
void rlc_um::handle_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  std::map<uint32_t, rlc_umd_pdu_t>::iterator it;
  rlc_umd_pdu_header_t header;
  rlc_um_read_data_pdu_header(payload, nof_bytes, rx_sn_field_length, &header);

  // log->info_hex(payload, nof_bytes, "RX %s Rx data PDU SN: %d",
  //               rb_id_text[lcid], header.sn);

  if(RX_MOD_BASE(header.sn) >= RX_MOD_BASE(vr_uh-rx_window_size) &&
     RX_MOD_BASE(header.sn) <  RX_MOD_BASE(vr_ur))
  {
    // log->info("%s SN: %d outside rx window [%d:%d] - discarding\n",
    //           rb_id_text[lcid], header.sn, vr_ur, vr_uh);
    return;
  }
  it = rx_window.find(header.sn);
  if(rx_window.end() != it)
  {
    // log->info("%s Discarding duplicate SN: %d\n",
    //           rb_id_text[lcid], header.sn);
    return;
  }

  // Write to rx window
  rlc_umd_pdu_t pdu;
  pdu.buf = pool->allocate();
  if (!pdu.buf) {
    log->error("Discarting packet: no space in buffer pool\n");
    return;
  }
  memcpy(pdu.buf->msg, payload, nof_bytes);
  pdu.buf->N_bytes = nof_bytes;
  //Strip header from PDU
  int header_len = rlc_um_packed_length(&header);
  pdu.buf->msg += header_len;
  pdu.buf->N_bytes -= header_len;
  pdu.header = header;
  rx_window[header.sn] = pdu;
  
  // Update vr_uh
  if(!inside_reordering_window(header.sn))
    vr_uh  = (header.sn + 1)%rx_mod;

  // Reassemble and deliver SDUs, while updating vr_ur
  log->debug("Entering Reassemble from received PDU\n");/////////////////////////
  reassemble_rx_sdus();
  log->debug("Finished reassemble from received PDU\n");
  
  // Update reordering variables and timers
  if(mac_timers->get(reordering_timeout_id)->is_running())
  {
    if(RX_MOD_BASE(vr_ux) <= RX_MOD_BASE(vr_ur) ||
       (!inside_reordering_window(vr_ux) && vr_ux != vr_uh))
    {
      mac_timers->get(reordering_timeout_id)->stop();
    }
  }
  if(!mac_timers->get(reordering_timeout_id)->is_running())
  {
    if(RX_MOD_BASE(vr_uh) > RX_MOD_BASE(vr_ur))
    {
      mac_timers->get(reordering_timeout_id)->set(this, t_reordering);
      mac_timers->get(reordering_timeout_id)->run();
      vr_ux = vr_uh;
    }
  }

  debug_state();
}

void rlc_um::reassemble_rx_sdus()
{ 
  if(!rx_sdu)
    rx_sdu = pool->allocate();
  
  // First catch up with lower edge of reordering window
  while(!inside_reordering_window(vr_ur))
  { 
    if(rx_window.end() == rx_window.find(vr_ur))
    {
      rx_sdu->reset();
    }else{
      // Handle any SDU segments
      for(int i=0; i<rx_window[vr_ur].header.N_li; i++)
      {
        int len = rx_window[vr_ur].header.li[i];
        memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, len);
        rx_sdu->N_bytes += len;
        rx_window[vr_ur].buf->msg += len;
        rx_window[vr_ur].buf->N_bytes -= len;
        if(pdu_lost && !rlc_um_start_aligned(rx_window[vr_ur].header.fi) || vr_ur != ((vr_ur_in_rx_sdu+1)%rx_mod)) {
          log->warning("Dropping remainder of lost PDU (lower edge middle segments, vr_ur=%d, vr_ur_in_rx_sdu=%d)\n", vr_ur, vr_ur_in_rx_sdu);
          rx_sdu->reset();
        } else {
          // log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU vr_ur=%d, i=%d (lower edge middle segments)", rb_id_text[lcid], vr_ur, i);
          rx_sdu->timestamp = bpt::microsec_clock::local_time();

          //pdcp->write_pdu(lcid, rx_sdu);
//pthread_mutex_lock(&mut);//lock
	cwrite(tun_fd, rx_sdu->msg, rx_sdu->N_bytes);
//pthread_mutex_unlock(&mut);//lock
	printf("write into tun \n");

          rx_sdu = pool->allocate();
        }
        pdu_lost = false;
      }

      // Handle last segment
      memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, rx_window[vr_ur].buf->N_bytes);
      rx_sdu->N_bytes += rx_window[vr_ur].buf->N_bytes;
      log->debug("Writting last segment in SDU buffer. Lower edge vr_ur=%d, Buffer size=%d, segment size=%d\n", 
               vr_ur, rx_sdu->N_bytes, rx_window[vr_ur].buf->N_bytes);
      vr_ur_in_rx_sdu = vr_ur; 
      if(rlc_um_end_aligned(rx_window[vr_ur].header.fi))
      {
        if(pdu_lost && !rlc_um_start_aligned(rx_window[vr_ur].header.fi)) {
          log->warning("Dropping remainder of lost PDU (lower edge last segments)\n");
          rx_sdu->reset();          
        } else {
          // log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU vr_ur=%d (lower edge last segments)", rb_id_text[lcid], vr_ur);
          rx_sdu->timestamp = bpt::microsec_clock::local_time();

          //pdcp->write_pdu(lcid, rx_sdu);
//pthread_mutex_lock(&mut);//lock
	cwrite(tun_fd, rx_sdu->msg, rx_sdu->N_bytes);
//pthread_mutex_unlock(&mut);//lock
	
	printf("write the last into tun!\n");

          rx_sdu = pool->allocate();
        }
        pdu_lost = false;
      }

      // Clean up rx_window
      pool->deallocate(rx_window[vr_ur].buf);
      rx_window.erase(vr_ur);
    }

    vr_ur = (vr_ur + 1)%rx_mod;
  }


  // Now update vr_ur until we reach an SN we haven't yet received
  while(rx_window.end() != rx_window.find(vr_ur))
  {//printf("come here \n");
    // Handle any SDU segments
    for(int i=0; i<rx_window[vr_ur].header.N_li; i++)
    {
      int len = rx_window[vr_ur].header.li[i];
      memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, len);
      log->debug("Concatenating %d bytes in to current length %d. rx_window remaining bytes=%d, vr_ur_in_rx_sdu=%d, vr_ur=%d, rx_mod=%d, last_mod=%d\n",
        len, rx_sdu->N_bytes, rx_window[vr_ur].buf->N_bytes, vr_ur_in_rx_sdu, vr_ur, rx_mod, (vr_ur_in_rx_sdu+1)%rx_mod);
      rx_sdu->N_bytes += len;      
      rx_window[vr_ur].buf->msg += len;
      rx_window[vr_ur].buf->N_bytes -= len;
      if(pdu_lost && !rlc_um_start_aligned(rx_window[vr_ur].header.fi) || vr_ur != ((vr_ur_in_rx_sdu+1)%rx_mod)) {
        log->warning("Dropping remainder of lost PDU (update vr_ur middle segments, vr_ur=%d, vr_ur_in_rx_sdu=%d)\n", vr_ur, vr_ur_in_rx_sdu);
        rx_sdu->reset();
      } else {
        // log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU vr_ur=%d, i=%d, (update vr_ur middle segments)", rb_id_text[lcid], vr_ur, i);
        rx_sdu->timestamp = bpt::microsec_clock::local_time();

        //pdcp->write_pdu(lcid, rx_sdu);
//pthread_mutex_lock(&mut);//lock
	cwrite(tun_fd, rx_sdu->msg, rx_sdu->N_bytes);
//pthread_mutex_unlock(&mut);//lock
	printf("write new into tun \n");
        rx_sdu = pool->allocate();
      }
      pdu_lost = false;
    }
    
    // Handle last segment
    memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, rx_window[vr_ur].buf->N_bytes);
    rx_sdu->N_bytes += rx_window[vr_ur].buf->N_bytes;
    log->debug("Writting last segment in SDU buffer. Updating vr_ur=%d, Buffer size=%d, segment size=%d\n", 
               vr_ur, rx_sdu->N_bytes, rx_window[vr_ur].buf->N_bytes);
    vr_ur_in_rx_sdu = vr_ur; 
    if(rlc_um_end_aligned(rx_window[vr_ur].header.fi))
    {
      if(pdu_lost && !rlc_um_start_aligned(rx_window[vr_ur].header.fi)) {
        log->warning("Dropping remainder of lost PDU (update vr_ur last segments)\n");
        rx_sdu->reset();
      } else {
        // log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU vr_ur=%d (update vr_ur last segments)", rb_id_text[lcid], vr_ur);
        rx_sdu->timestamp = bpt::microsec_clock::local_time();

        //pdcp->write_pdu(lcid, rx_sdu);
//pthread_mutex_lock(&mut);//lock
	cwrite(tun_fd, rx_sdu->msg, rx_sdu->N_bytes);
//pthread_mutex_unlock(&mut);//lock
printf("write the last into tun \n");

        rx_sdu = pool->allocate();
      }
      pdu_lost = false;
    }

    // Clean up rx_window
    pool->deallocate(rx_window[vr_ur].buf);
    rx_window.erase(vr_ur);

    vr_ur = (vr_ur + 1)%rx_mod;
  }
}

bool rlc_um::inside_reordering_window(uint16_t sn)
{
  if(RX_MOD_BASE(sn) >= RX_MOD_BASE(vr_uh-rx_window_size) &&
     RX_MOD_BASE(sn) <  RX_MOD_BASE(vr_uh))
  {
    return true;
  }else{
    return false;
  }
}

void rlc_um::debug_state()
{
  log->debug("%s vt_us = %d, vr_ur = %d, vr_ux = %d, vr_uh = %d \n",
             rb_id_text[lcid], vt_us, vr_ur, vr_ux, vr_uh);

}

/****************************************************************************
 * Header pack/unpack helper functions
 * Ref: 3GPP TS 36.322 v10.0.0 Section 6.2.1
 ***************************************************************************/

void rlc_um_read_data_pdu_header(byte_buffer_t *pdu, rlc_umd_sn_size_t sn_size, rlc_umd_pdu_header_t *header)
{
  rlc_um_read_data_pdu_header(pdu->msg, pdu->N_bytes, sn_size, header);
}

void rlc_um_read_data_pdu_header(uint8_t *payload, uint32_t nof_bytes, rlc_umd_sn_size_t sn_size, rlc_umd_pdu_header_t *header)
{
  uint8_t  ext;
  uint8_t *ptr = payload;

  // Fixed part
  if(RLC_UMD_SN_SIZE_5_BITS == sn_size)
  {
    header->fi = (rlc_fi_field_t)((*ptr >> 6) & 0x03);  // 2 bits FI
    ext        =                 ((*ptr >> 5) & 0x01);  // 1 bit EXT
    header->sn =                 *ptr & 0x1F;           // 5 bits SN
    ptr++;
  }else{
    header->fi = (rlc_fi_field_t)((*ptr >> 3) & 0x03);  // 2 bits FI
    ext        =                 ((*ptr >> 2) & 0x01);  // 1 bit EXT
    header->sn =                 (*ptr & 0x03) << 8;    // 2 bits SN
    ptr++;
    header->sn |=                (*ptr & 0xFF);         // 8 bits SN
    ptr++;
  }

  header->sn_size = sn_size;

  // Extension part
  header->N_li = 0;
  while(ext)
  {
    if(header->N_li%2 == 0)
    {
      ext = ((*ptr >> 7) & 0x01);
      header->li[header->N_li]  = (*ptr & 0x7F) << 4; // 7 bits of LI
      ptr++;
      header->li[header->N_li] |= (*ptr & 0xF0) >> 4; // 4 bits of LI
      header->N_li++;
    }
    else
    {
      ext = (*ptr >> 3) & 0x01;
      header->li[header->N_li] = (*ptr & 0x07) << 8; // 3 bits of LI
      ptr++;
      header->li[header->N_li] |= (*ptr & 0xFF);     // 8 bits of LI
      header->N_li++;
      ptr++;
    }
  }
}

void rlc_um_write_data_pdu_header(rlc_umd_pdu_header_t *header, byte_buffer_t *pdu)
{
  uint32_t i;
  uint8_t ext = (header->N_li > 0) ? 1 : 0;

  // Make room for the header
  uint32_t len = rlc_um_packed_length(header);
  pdu->msg -= len;
  uint8_t *ptr = pdu->msg;

  // Fixed part
  if(RLC_UMD_SN_SIZE_5_BITS == header->sn_size)
  {
    *ptr  = (header->fi & 0x03) << 6;   // 2 bits FI
    *ptr |= (ext        & 0x01) << 5;   // 1 bit EXT
    *ptr |= header->sn  & 0x1F;         // 5 bits SN
    ptr++;
  }else{
    *ptr  = (header->fi & 0x03) << 3;   // 3 Reserved bits | 2 bits FI
    *ptr |= (ext        & 0x01) << 2;   // 1 bit EXT
    *ptr |= (header->sn & 0x300) >> 8;  // 2 bits SN
    ptr++;
    *ptr  = (header->sn & 0xFF);        // 8 bits SN
    ptr++;
  }

  // Extension part
  i = 0;
  while(i < header->N_li)
  {
    ext = ((i+1) == header->N_li) ? 0 : 1;
    *ptr  = (ext           &  0x01) << 7; // 1 bit header
    *ptr |= (header->li[i] & 0x7F0) >> 4; // 7 bits of LI
    ptr++;
    *ptr  = (header->li[i] & 0x00F) << 4; // 4 bits of LI
    i++;
    if(i < header->N_li)
    {
      ext = ((i+1) == header->N_li) ? 0 : 1;
      *ptr |= (ext           &  0x01) << 3; // 1 bit header
      *ptr |= (header->li[i] & 0x700) >> 8; // 3 bits of LI
      ptr++;
      *ptr  = (header->li[i] & 0x0FF);      // 8 bits of LI
      ptr++;
      i++;
    }
  }
  // Pad if N_li is odd
  if(header->N_li%2 == 1)
    ptr++;

  pdu->N_bytes += ptr-pdu->msg;
}

uint32_t rlc_um_packed_length(rlc_umd_pdu_header_t *header)
{
  uint32_t len = 0;
  if(RLC_UMD_SN_SIZE_5_BITS == header->sn_size)
  {
    len += 1; // Fixed part is 1 byte
  }else{
    len += 2; // Fixed part is 2 bytes
  }
  len += header->N_li * 1.5 + 0.5;  // Extension part - integer rounding up
  return len;
}

bool rlc_um_start_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_END_ALIGNED);
}

bool rlc_um_end_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_START_ALIGNED);
}


} // namespace srsue
