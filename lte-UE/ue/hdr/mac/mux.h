#ifndef MUX_H
#define MUX_H

#include <pthread.h>

#include "common/qbuff.h"
#include "common/log.h"
#include "common/mac_interface.h"
#include "common/pdu.h"
#include "mac/proc_bsr.h"
#include "mac/proc_phr.h"
#include "upper/rlc_um.h"
/* Logical Channel Multiplexing and Prioritization + Msg3 Buffer */   

//using namespace srslte;

namespace srsue {
  
class mux
{
public:
  mux();
  void     reset();
 void     init(rlc_interface_mac *rlc, srslte::log *log_h, bsr_proc *bsr_procedure, phr_proc *phr_procedure_);
//void     init(rlc_um *rlc, srslte::log *log_h, bsr_proc *bsr_procedure, phr_proc *phr_procedure_);
  bool     is_pending_any_sdu();
  bool     is_pending_sdu(uint32_t lcid); 
  
  uint8_t* pdu_get(uint8_t *payload, uint32_t pdu_sz, uint32_t tx_tti, uint32_t pid);
  uint8_t* msg3_get(uint8_t* payload, uint32_t pdu_sz);
  
  void     msg3_flush();
  bool     msg3_is_transmitted();
  
  void     append_crnti_ce_next_tx(uint16_t crnti); 
  
  void     set_priority(uint32_t lcid, uint32_t priority, int PBR_x_tti, uint32_t BSD);
  void     pusch_retx(uint32_t tx_tti, uint32_t pid);
      
private:  
  bool     pdu_move_to_msg3(uint32_t pdu_sz);
  bool     allocate_sdu(uint32_t lcid, srslte::sch_pdu *pdu, int max_sdu_sz, uint32_t *sdu_sz);
  
  // There is a known bug in the code and NOF_UL_LCH must match the maximum priority (16) + 1
  const static int NOF_UL_LCH = 17; 
  const static int MIN_RLC_SDU_LEN = 0; 
  const static int MAX_NOF_SUBHEADERS = 20; 
  const static int MAX_HARQ_PROC = 8; 
  
  int64_t       Bj[NOF_UL_LCH];
  int           PBR[NOF_UL_LCH]; // -1 sets to infinity
  uint32_t      BSD[NOF_UL_LCH];
  uint32_t      priority[NOF_UL_LCH];
  uint32_t      priority_sorted[NOF_UL_LCH];
  uint32_t      lchid_sorted[NOF_UL_LCH];
  
  // Keep track of the PIDs that transmitted BSR reports 
  bool pid_has_bsr[MAX_HARQ_PROC]; 
  
  // Mutex for exclusive access
  pthread_mutex_t mutex; 

  srslte::log       *log_h;
  rlc_interface_mac *rlc; 
//rlc_um *rlc;
  bsr_proc          *bsr_procedure;
  phr_proc          *phr_procedure;
  uint16_t           pending_crnti_ce;
  
  /* Msg3 Buffer */
  static const uint32_t MSG3_BUFF_SZ = 128; 
  srslte::qbuff         msg3_buff;
  
  /* PDU Buffer */
  srslte::sch_pdu    pdu_msg; 
  bool msg3_has_been_transmitted;
  
  
  
};

} // namespace srsue

#endif // MUX_H

