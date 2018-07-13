#include "upper/rlc_tm.h"

using namespace srslte;

namespace srsue{

rlc_tm::rlc_tm() : ul_queue(16)
{
  pool = buffer_pool::get_instance();
}

void rlc_tm::init(srslte::log        *log_,
                  uint32_t            lcid_,
                  pdcp_interface_rlc *pdcp_,
                  rrc_interface_rlc  *rrc_, 
                  mac_interface_timers *mac_timers)
{
  log  = log_;
  lcid = lcid_;
  pdcp = pdcp_;
  rrc  = rrc_;
}

void rlc_tm::configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg)
{
  log->error("Attempted to configure TM RLC entity");
}

void rlc_tm::empty_queue()
{
  // Drop all messages in TX queue
  byte_buffer_t *buf;
  while(ul_queue.size() > 0) {
    ul_queue.read(&buf);
    pool->deallocate(buf);
  }
}

void rlc_tm::reset()
{
  empty_queue(); 
}

rlc_mode_t rlc_tm::get_mode()
{
  return RLC_MODE_TM;
}

uint32_t rlc_tm::get_bearer()
{
  return lcid;
}

// PDCP interface
void rlc_tm::write_sdu(byte_buffer_t *sdu)
{
  log->info_hex(sdu->msg, sdu->N_bytes, "%s Tx SDU", rb_id_text[lcid]);
  ul_queue.write(sdu);
}

// MAC interface
uint32_t rlc_tm::get_buffer_state()
{
  return ul_queue.size_bytes();
}

uint32_t rlc_tm::get_total_buffer_state()
{
  return get_buffer_state();
}

int rlc_tm::read_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  uint32_t pdu_size = ul_queue.size_tail_bytes();
  if(pdu_size > nof_bytes)
  {
    log->error("TX %s PDU size larger than MAC opportunity\n", rb_id_text[lcid]);
    return 0;
  }
  byte_buffer_t *buf;
  ul_queue.read(&buf);
  pdu_size = buf->N_bytes;
  memcpy(payload, buf->msg, buf->N_bytes);
  log->info("%s Complete SDU scheduled for tx. Stack latency: %ld us\n",
            rb_id_text[lcid], buf->get_latency_us());
  pool->deallocate(buf);
  log->info_hex(payload, pdu_size, "TX %s, %s PDU", rb_id_text[lcid], rlc_mode_text[RLC_MODE_TM]);
  return pdu_size;
}

void rlc_tm:: write_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  byte_buffer_t *buf = pool->allocate();
  memcpy(buf->msg, payload, nof_bytes);
  buf->N_bytes = nof_bytes;
  buf->timestamp = bpt::microsec_clock::local_time();
  pdcp->write_pdu(lcid, buf);  
}

} // namespace srsue
