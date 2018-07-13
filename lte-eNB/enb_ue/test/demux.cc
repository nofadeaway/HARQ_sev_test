#define Error(fmt, ...)   log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

//#include "mac/mac.h"----修改
#include "mac/demux.h"

namespace srsue {
    
demux::demux() : mac_msg(20), pending_mac_msg(20)
{
}

//初始化解包实体
void demux::init(phy_interface_mac* phy_h_, rlc_interface_mac *rlc_, srslte::log* log_h_)//修改, srslte::timers* timers_db_)
{
  phy_h     = phy_h_; 
  log_h     = log_h_; 
  rlc       = rlc_;  
  //timers_db = timers_db_;修改
  pdus.init(this, log_h);
}

void demux::set_uecrid_callback(bool (*callback)(void*,uint64_t), void *arg) {
  uecrid_callback     = callback;
  uecrid_callback_arg = arg; 
}

bool demux::get_uecrid_successful() {
  return is_uecrid_successful;
}

uint8_t* demux::request_buffer(uint32_t pid, uint32_t len)
{  
  uint8_t *buff = NULL; 
  if (pid < NOF_HARQ_PID) {
    return pdus.request_buffer(pid, len);
  } else if (pid == NOF_HARQ_PID) {
    buff = bcch_buffer;
  } else {
    Error("Requested buffer for invalid PID=%d\n", pid);
  }
  return buff; 
}

/* Demultiplexing of MAC PDU associated with a Temporal C-RNTI. The PDU will 
 * remain in buffer until demultiplex_pending_pdu() is called. 
 * This features is provided to enable the Random Access Procedure to decide 
 * wether the PDU shall pass to upper layers or not, which depends on the 
 * Contention Resolution result. 
 * 
 * Warning: this function does some processing here assuming ACK deadline is not an 
 * issue here because Temp C-RNTI messages have small payloads
 */
void demux::push_pdu_temp_crnti(uint32_t pid, uint8_t *buff, uint32_t nof_bytes) 
{
  if (pid < NOF_HARQ_PID) {
    if (nof_bytes > 0) {
      // Unpack DLSCH MAC PDU 
      pending_mac_msg.init_rx(nof_bytes);
      pending_mac_msg.parse_packet(buff);
      
      // Look for Contention Resolution UE ID 
      is_uecrid_successful = false; 
      while(pending_mac_msg.next() && !is_uecrid_successful) {
        if (pending_mac_msg.get()->ce_type() == srslte::sch_subh::CON_RES_ID) {
          Debug("Found Contention Resolution ID CE\n");
          is_uecrid_successful = uecrid_callback(uecrid_callback_arg, pending_mac_msg.get()->get_con_res_id());
        }
      }
      
      pending_mac_msg.reset();
      
      Debug("Saved MAC PDU with Temporal C-RNTI in buffer\n");
      
      pdus.push_pdu(pid, nof_bytes);
    } else {
      Warning("Trying to push PDU with payload size zero\n");
    }
  } else {
    Error("Pushed buffer for invalid PID=%d\n", pid);
  } 
}

/* Demultiplexing of logical channels and dissassemble of MAC CE 
 * This function enqueues the packet and returns quicly because ACK 
 * deadline is important here. 
 */ 
void demux::push_pdu(uint32_t pid, uint8_t *buff, uint32_t nof_bytes)
{
  if (pid < NOF_HARQ_PID) {    
    return pdus.push_pdu(pid, nof_bytes);
  } else if (pid == NOF_HARQ_PID) {
    /* Demultiplexing of MAC PDU associated with SI-RNTI. The PDU passes through 
    * the MAC in transparent mode. 
    * Warning: In this case function sends the message to RLC now, since SI blocks do not 
    * require ACK feedback to be transmitted quickly. 
    */
    Debug("Pushed BCCH MAC PDU in transparent mode\n");
    //rlc->write_pdu_bcch_dlsch(buff, nof_bytes);修改
  } else {
    Error("Pushed buffer for invalid PID=%d\n", pid);
  }  
}

bool demux::process_pdus()
{
  return pdus.process_pdus();
}

//Unpack DLSCH MAC PDU 
void demux::process_pdu(uint8_t *mac_pdu, uint32_t nof_bytes)
{
  // Unpack DLSCH MAC PDU 
  mac_msg.init_rx(nof_bytes);
  mac_msg.parse_packet(mac_pdu);

  process_sch_pdu(&mac_msg);//124
  //srslte_vec_fprint_byte(stdout, mac_pdu, nof_bytes);
//printf("test MAC PDU processed!\n");
  Debug("MAC PDU processed\n");
}

void demux::process_sch_pdu(srslte::sch_pdu *pdu_msg)////////////////主要解包程序
{  
  while(pdu_msg->next()) {
    if (pdu_msg->get()->is_sdu()) {	

	printf("\n\n\n");

      // Route logical channel 
      Info("Delivering PDU for lcid=%d, %d bytes\n", pdu_msg->get()->get_sdu_lcid(), pdu_msg->get()->get_payload_size());
      rlc->write_pdu(pdu_msg->get()->get_sdu_lcid(), pdu_msg->get()->get_sdu_ptr(), pdu_msg->get()->get_payload_size());      
    } //else {//不解控制信息包
      // Process MAC Control Element
    // if (!process_ce(pdu_msg->get())) {
        //Warning("Received Subheader with invalid or unkonwn LCID\n");
      //}
    //}
  }      
}

bool demux::process_ce(srslte::sch_subh *subh) {//这个地方应该要修改啊啊结构体类型---switch分类获得信息之后做什么呢，我知道什么不知道如何做啊
printf("1test HERE!\n");
  switch(subh->ce_type()) {printf("test HERE!\n");
    case srslte::sch_subh::CON_RES_ID:
      // Do nothing
      break;
    case srslte::sch_subh::TA_CMD:
      ///phy_h->set_timeadv(subh->get_ta_cmd());---phy注释掉了
      Info("Received TA=%d\n", subh->get_ta_cmd());
      
      // Start or restart timeAlignmentTimer
      //timers_db->get(mac::TIME_ALIGNMENT)->reset();修改
      //timers_db->get(mac::TIME_ALIGNMENT)->run();      修改
      break;
    case srslte::sch_subh::PADDING:
      break;
    default:
      Error("MAC CE 0x%x not supported\n", subh->ce_type());
      break;
  }printf("2test HERE!\n");
  return true; 
}


}
