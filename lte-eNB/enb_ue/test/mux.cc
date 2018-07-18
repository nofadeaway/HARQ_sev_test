#define Error(fmt, ...)   log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#include "mac/mux.h"
//#include "mac/mac.h"/////////////////////////////////////////////修改

using namespace srslte;
namespace srsue {

mux::mux() : pdu_msg(MAX_NOF_SUBHEADERS)
{
  msg3_buff.init(1, MSG3_BUFF_SZ);

  pthread_mutex_init(&mutex, NULL);
  msg3_has_been_transmitted = false; 
  
  for (int i=0;i<NOF_UL_LCH;i++) {
   priority[i]        = i; 
   priority_sorted[i] = i; 
   PBR[i]             = -1; // -1 is infinite 
   BSD[i]             = 10;
   lchid_sorted[i]    = i; 
  }  
  pending_crnti_ce = 0;
}

void mux::init(rlc_interface_mac *rlc_, srslte::log *log_h_, bsr_proc *bsr_procedure_, phr_proc *phr_procedure_)//xiugai
{
  log_h      = log_h_;
  rlc        = rlc_;
  bsr_procedure = bsr_procedure_;
  phr_procedure = phr_procedure_;
  //printf("Mux process succeeds!\n");
}

void mux::reset()
{
  for (int i=0;i<NOF_UL_LCH;i++) {
    Bj[i] = 0; 
  }
  pending_crnti_ce = 0;
}

bool mux::is_pending_any_sdu()
{
  for (int i=0;i<NOF_UL_LCH;i++) {
    if (rlc->get_buffer_state(i)) {
      return true; 
    }
  }
  return false; 
}

bool mux::is_pending_sdu(uint32_t lch_id) {
  return rlc->get_buffer_state(lch_id)>0;  
}

void mux::set_priority(uint32_t lch_id, uint32_t new_priority, int set_PBR, uint32_t set_BSD)
{
  if (lch_id < NOF_UL_LCH) {
    if (new_priority < NOF_UL_LCH) {
      priority[lch_id] = new_priority;
      PBR[lch_id]      = set_PBR;
      BSD[lch_id]      = set_BSD; 
      
      // Insert priority in sorted idx array
      int new_index = 0; 
      while(new_index < NOF_UL_LCH && new_priority > priority_sorted[new_index]) {
        new_index++; 
      }
      int old_index = 0; 
      while(old_index < NOF_UL_LCH && lch_id != lchid_sorted[old_index]) {
        old_index++;
      }
      if (new_index ==  NOF_UL_LCH) {
        Error("Can't find LCID=%d in sorted list\n", lch_id);
        return;
      }
      // Replace goes in one direction or the other 
      int add=new_index>old_index?1:-1;
      for (int i=old_index;i!=new_index;i+=add) {
        priority_sorted[i] = priority_sorted[i+add];
        lchid_sorted[i]    = lchid_sorted[i+add];
      }
      priority_sorted[new_index] = new_priority;
      lchid_sorted[new_index]    = lch_id; 
    } else {
      Error("Invalid priority %d: must be in 1..16\n", new_priority);
    }
  } else {
    Error("Invalid lch_id %d: must be in 0..15\n", lch_id);
  }
}

srslte::sch_subh::cetype bsr_format_convert(bsr_proc::bsr_format_t format) {
  switch(format) {
    case bsr_proc::LONG_BSR: 
      return srslte::sch_subh::LONG_BSR;
    case bsr_proc::SHORT_BSR: 
      return srslte::sch_subh::SHORT_BSR;
    case bsr_proc::TRUNC_BSR: 
      return srslte::sch_subh::TRUNC_BSR;   
  }
}

void mux::pusch_retx(uint32_t tx_tti, uint32_t pid)
{
  if (pid_has_bsr[pid%MAX_HARQ_PROC]) {
    bsr_procedure->set_tx_tti(tx_tti);
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multiplexing and logical channel priorization as defined in Section 5.4.3
uint8_t* mux::pdu_get(uint8_t *payload, uint32_t pdu_sz, uint32_t tx_tti, uint32_t pid)
{	
  printf("\n\n");
  printf("---------------------------------------\n");
  printf("------------start_pdu_get--------------\n");
  
  pthread_mutex_lock(&mutex);
    
  // Update Bj
  for (int i=0;i<NOF_UL_LCH;i++) {    
    // Add PRB unless it's infinity 
    if (PBR[i] >= 0) {
      Bj[i] += PBR[i];
    }
    if (Bj[i] >= BSD[i]) {
      Bj[i] = BSD[i]; 
    }    
  }
 
// Logical Channel Procedure修改-----组包一个没有完整控制信息的SDU包

  pdu_msg.init_tx(payload, pdu_sz, true);        //pdu.h 77行

  // MAC control element for C-RNTI or data from UL-CCCH-----但是现在我们不接收来自CCCH的数据
  if (!allocate_sdu(0, &pdu_msg, -1, NULL)) {
    //printf("pending_crnti_ce is %d",pending_crnti_ce);
    if (pending_crnti_ce) {
      if (pdu_msg.new_subh()) {
        if (!pdu_msg.get()->set_c_rnti(pending_crnti_ce)) {
          Warning("Pending C-RNTI CE could not be inserted in MAC PDU\n");
        }
      }
    }
  } 
  pending_crnti_ce = 0; 
  /* 
  bsr_proc::bsr_t bsr; 
  //修改bool regular_bsr = bsr_procedure->need_to_send_bsr_on_ul_grant(pdu_msg.rem_size(), &bsr);printf("HERE32\n");
bool regular_bsr = true;  
bool bsr_is_inserted = false; 
   
  // MAC control element for BSR, with exception of BSR included for padding;
  if (regular_bsr) {
    if (pdu_msg.new_subh()) {
      pdu_msg.get()->set_bsr(bsr.buff_size, bsr_format_convert(bsr.format));    
      bsr_is_inserted  = true; 
    }
  }*/
  // MAC control element for PHR
  //float phr_value;
  //phr_procedure->generate_phr_on_ul_grant(&phr_value);
  // if (phr_procedure->generate_phr_on_ul_grant(&phr_value)) {
  //   if (pdu_msg.new_subh()) {
  //     pdu_msg.get()->set_phr(phr_value);
  //   }
  // }
  
  // data from any Logical Channel, except data from UL-CCCH;  
  // first only those with positive Bj
  uint32_t sdu_sz   = 0; 
  for (int i=3;i<NOF_UL_LCH;i++) {
    uint32_t lcid = lchid_sorted[i];
    if (lcid != 0) {
      bool res = true; 
      while ((Bj[lcid] > 0 || PBR[lcid] < 0) && res) {
        res = allocate_sdu(lcid, &pdu_msg, (PBR[lcid]<0)?-1:Bj[lcid], &sdu_sz);//////////////HERE SDU INTO
        if (res && PBR[lcid] >= 0) {
          Bj[lcid] -= sdu_sz;         
        }
      }
    }
  } 
/*
  // If resources remain, allocate regardless of their Bj value
  for (int i=1;i<NOF_UL_LCH;i++) {
    while (allocate_sdu(lchid_sorted[i], &pdu_msg, -1, NULL));   
  }printf("HERE6\n");

  if (!regular_bsr) {
    // Insert Padding BSR if not inserted Regular/Periodic BSR 
    if (bsr_procedure->generate_padding_bsr(pdu_msg.rem_size(), &bsr)) {
      if (pdu_msg.new_subh()) {
        pdu_msg.get()->set_bsr(bsr.buff_size, bsr_format_convert(bsr.format));
        bsr_is_inserted  = true; 
      }    
    }
  }printf("HERE7\n");
*/ 
  
  // int muc_result=pdu_msg.get_pdu_len()-pdu_msg.rem_size();
  // if(muc_result == 0)
  //    return NULL;
  //printf("pdu_msg.get_pdu_len() is %d.\n",pdu_msg.get_pdu_len());
  //printf("pdu_msg.rem_size() is %d.\n",pdu_msg.rem_size());
  // if(pdu_msg.get_pdu_len()==pdu_msg.rem_size())
  // {
  //   return NULL;
  // }
  Debug("Assembled MAC PDU msg size %d/%d bytes\n", pdu_msg.get_pdu_len()-pdu_msg.rem_size(), pdu_sz);
 
  /* Generate MAC PDU and save to buffer */
  uint8_t *ret = pdu_msg.write_packet(log_h);          //pdu.cc 114
printf("------------end_pdu_get----------------\n");
printf("---------------------------------------\n");
 //pid_has_bsrpid_has_bsr[pid%MAX_HARQ_PROC] = bsr_is_inserted; 
 // if (bsr_is_inserted) {
 //   bsr_procedure->set_tx_tti(tx_tti);
 // }
  
  pthread_mutex_unlock(&mutex);
  

  return ret; 
}

void mux::append_crnti_ce_next_tx(uint16_t crnti) {
  pending_crnti_ce = crnti; 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
bool mux::allocate_sdu(uint32_t lcid, srslte::sch_pdu* pdu_msg, int max_sdu_sz, uint32_t* sdu_sz)  //从rlc拉取数据
{ 
 
  // Get n-th pending SDU pointer and length
  int sdu_len = rlc->get_buffer_state(lcid); 
  //if (sdu_len > 0 && sdu_len<43855) {
  if (sdu_len > 0 ) { // there is pending SDU to allocate小于一个数而不是没有数字

    //printf("\nhere_lcid = %d,max_sdu_sz = %d,rlc_buffer_sdu_len = %d\n",lcid,max_sdu_sz,sdu_len);///

    int buffer_state = sdu_len; 
     //printf("\n\n!!!Now sdu_len is %d!!!\n\n",sdu_len);    
    if (sdu_len > max_sdu_sz && max_sdu_sz >= 0) {    //-1 means infinity
      sdu_len = max_sdu_sz;
    }
    //printf("\n\n!!!Now max_sdu_sz is %d!!!\n\n",max_sdu_sz);   
    int sdu_space = pdu_msg->get_sdu_space();
    //printf("\n\n!!!Now sdu_space is %d!!!\n\n",sdu_space); 
    if (sdu_len > sdu_space) {
      sdu_len = sdu_space;
    }    
    //printf("\n\n!!!Now sdu_len is %d!!!\n\n",sdu_len);    
    if (sdu_len > MIN_RLC_SDU_LEN) {
      if (pdu_msg->new_subh()) { // there is space for a new subheader
       // int sdu_len2 = sdu_len;这一步一点用都没有啊 

//printf("\nrequst_mac_sdu_len = %d\n\n",sdu_len);

        sdu_len = pdu_msg->get()->set_sdu(lcid, sdu_len, rlc);////////////////////////SET SDU pdu.cc-541

//printf("\nfinal_mac_sdu_len = %d\n\n",sdu_len);//

        if (sdu_len > 0) { // new SDU could be added
          if (sdu_sz) {
            *sdu_sz = sdu_len; 
          }
          
          Info("SDU: rlc_buffer=%d, allocated=%d/%d, max_sdu_sz=%d, remaining=%d\n", 
                 buffer_state, sdu_len, sdu_space, max_sdu_sz, pdu_msg->rem_size());
          return true;               
        } else {
          Warning("SDU: rlc_buffer=%d, allocated=%d/%d, remaining=%d\n", 
               buffer_state, sdu_len, sdu_space, pdu_msg->rem_size());
          pdu_msg->del_subh();
        }
      } 
    }
  }
  return false; 
}

void mux::msg3_flush()
{
  Debug("Msg3 buffer flushed\n");
  msg3_buff.flush();
  msg3_has_been_transmitted = false; 
}

bool mux::msg3_is_transmitted()
{
  return msg3_has_been_transmitted; 
}


bool mux::pdu_move_to_msg3(uint32_t pdu_sz)
{
  uint8_t *msg3_start = (uint8_t*) msg3_buff.request();
  if (msg3_start) {
    uint8_t *msg3_pdu = pdu_get(msg3_start, pdu_sz, 0, 0); 
    if (msg3_pdu) {
      memmove(msg3_start, msg3_pdu, pdu_sz*sizeof(uint8_t));
      msg3_buff.push(pdu_sz);
      return true;       
    } else {
      Error("Assembling PDU\n");
    }    
  } else {
    Error("Generating PDU: PDU pending in buffer for transmission\n");
  }  
  return false; 
}

/* Returns a pointer to the Msg3 buffer */
uint8_t* mux::msg3_get(uint8_t *payload, uint32_t pdu_sz)
{
  if (msg3_buff.isempty()) {
    Debug("Moving PDU from Mux unit to Msg3 buffer\n");
    if (!pdu_move_to_msg3(pdu_sz)) {
      Error("Moving PDU from Mux unit to Msg3 buffer\n");
      return NULL;
    }    
  }
  uint8_t *msg3 = (uint8_t*) msg3_buff.pop();
  if (msg3) {
    memcpy(payload, msg3, sizeof(uint8_t)*pdu_sz);
    msg3_has_been_transmitted = true; 
    return payload; 
  } else {
    Error("Generating Msg3\n");
  }
  return NULL; 
}  
}
