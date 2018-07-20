#ifndef FX_MAC_H
#define FX_MAC_H

#define Error(fmt, ...) log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...) log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SEND_SIZE 400
#define HARQ_NUM 8
#define SIZE_RLC_QUEUE 24

#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <map>

#include "FuncHead.h"
#include "../hdr/upper/rlc_um.h"
#include "../hdr/common/log.h"
#include "../hdr/mac/mac.h"
#include "../hdr/common/pcap.h"

using namespace srslte;
using namespace srsue;

class mac_interface_phy_FX
{
public:
  typedef struct
  {
    uint32_t pid;
    uint32_t tti;
    uint32_t last_tti;
    bool ndi;
    bool last_ndi;
    uint32_t n_bytes;
    int rv;
    uint16_t rnti;
    bool is_from_rar;
    bool is_sps_release;
    bool has_cqi_request;
    srslte_rnti_type_t rnti_type;
    srslte_phy_grant_t phy_grant;
  } mac_grant_t;

  typedef struct
  {
    bool decode_enabled;
    int rv;
    uint16_t rnti;
    bool generate_ack;
    bool default_ack;
    // If non-null, called after tb_decoded_ok to determine if ack needs to be sent
    bool (*generate_ack_callback)(void *);
    void *generate_ack_callback_arg;
    uint8_t *payload_ptr;
    srslte_softbuffer_rx_t *softbuffer;
    srslte_phy_grant_t phy_grant;
  } tb_action_dl_t;

  typedef struct
  {
    bool tx_enabled;
    bool expect_ack;
    uint32_t rv;
    uint16_t rnti;
    uint32_t current_tx_nb;
    srslte_softbuffer_tx_t *softbuffer;
    srslte_phy_grant_t phy_grant;
    uint8_t *payload_ptr;
  } tb_action_ul_t;
};

/**************************************************************************
* rlc-class
**************************************************************************/

class UE_process_FX //处理之前的程序
{
public:
  //rlc_um_tester_3 i_rlc3;
  uint16_t rnti;
  pthread_mutex_t ACK_LOCK;
  pthread_mutex_t busy_LOCK;
  pthread_cond_t busy_signal;
  uint32_t busy_num;
  //mac_dummy_timers timers_test;
  mux ue_mux_test;
  demux mac_demux_test;
  //demux mac_demux_test_trans;       //用于发送方的,其中自动会有pdu_queue
  srslte::pdu_queue pdu_queue_test; //自己添加的PDU排队缓存,目前支持的HARQ进程数最多为8,既最多缓存8个PDU队列
  bool ACK[HARQ_NUM] = {false, false, false, false, false, false, false, false};
  bool ACK_default[HARQ_NUM] = {false, false, false, false, false, false, false, false};
  bool busy_index[HARQ_NUM] = {false, false, false, false, false, false, false, false};

  void *pdu_store(uint32_t pid_now, uint8_t *payload_back, uint32_t pdu_sz_test)
  {
    if (!payload_back)
    {
      printf("RNTI:%d:::No data to store in queue.\n", rnti);
      return NULL;
    }
    bool qbuff_flag;
    //payload_back = ue_mux_test.pdu_get(payload_test, pdu_sz_test, tx_tti_test, pid_now);
    printf("RNTI:%d:::Now this pdu belongs to HARQ NO.%d\n", rnti, pid_now);

    //begin{5.28添加}
    //if(pdu_queue_test.request_buffer(pid_now,pdu_sz_test))     //request_buffer函数返回指为qbuff中ptr指针,而在下面send中其实并不需要使用
    //{printf("PID No.%d:queue's buffer request succeeded!\n",pid_now);}

    qbuff_flag = pdu_queue_test.pdu_q[pid_now].send(payload_back, pdu_sz_test); //把payload)back存入qbuff
    if (qbuff_flag)
    {
      printf("RNTI:%d:::Succeed in sending PDU to queue's buffer!\n", rnti);
    }
    else
    {
      printf("RNTI:%d:::Fail in sending PDU to queue's buffer!\n", rnti);
    }
  }

  bool pdu_in(uint8_t *payload_back, uint32_t pdu_sz_test) // 将pdu存入为空闲状态的HARQ进程的队列，但只查找一次
  {
    pthread_mutex_lock(&busy_LOCK);
    static uint32_t pid_not_busy = 0;
    if (busy_num < HARQ_NUM) //有空闲的就存入空闲状态的
    {
      while (busy_index[pid_not_busy])
      {
        pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
      }
    }
    else //无空闲的随便存
    {
      pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
    }
    pthread_mutex_unlock(&busy_LOCK);
    if (pdu_store(pid_not_busy, payload_back, pdu_sz_test))
      return true;
    else
      return false;
  }
  void reset(uint32_t pid)
  {
    pthread_mutex_lock(&busy_LOCK);
    busy_index[pid] = false;
    busy_num--;
    pthread_mutex_unlock(&busy_LOCK);
  }
  void ack_recv(uint32_t pid)
  {
    pthread_mutex_lock(&busy_LOCK);
    busy_index[pid] = false;
    busy_num--;
    pthread_cond_signal(&busy_signal);
    pthread_mutex_unlock(&busy_LOCK);
  }

  uint8_t *trans_control(uint32_t pid_now, uint32_t len)
  {
    int retrans_limit = 3;
    static int retrans_times[8] = {0};
    //begin{5.29}
    //uint8_t *payload_tosend = new uint8_t[SEND_SIZE];
    //bool qbuff_flag=false;   //记录 qbuff::send()返回值
    //end{5.29}

    printf("RNTI:%d::: Now this pdu belongs to HARQ NO.%d\n", rnti, pid_now);

    /***********************************************
	    *控制重发
	    *************************************************/
    pthread_mutex_lock(&ACK_LOCK);
    bool ACK_temp = ACK[pid_now];
    pthread_mutex_unlock(&ACK_LOCK);
    // if(pdu_queue_test.pdu_q[pid_now].isempty())         //不需要,pop函数里面,若队列空,会返回NUll
    // {
    //   printf("RNTI %d::: No PDU in queue!\n",rnti);
    //   return NULL;
    // }
    if (ACK_temp)
    {
      //payload_tosend = payload_back;

      pdu_queue_test.pdu_q[pid_now].release();                                                                                       //将前一个已经收到对应ACK的PDU丢弃
      printf("RNTI:%d::: Now PID No.%d:queue's No.%d buffer will be sent.\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is()); //接收到ACK,发送下一个PDU
      return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len);
    }
    else //没收到ACK,重发
    {
      //memcpy(payload_tosend, pdu_queue_test.pdu_q[pid_now].pop(pdu_sz_test,1), pdu_sz_test);
      retrans_times[pid_now]++;

      if (retrans_times[pid_now] < retrans_limit)
      {
        //uint32_t len=pdu_sz_test;
        //payload_tosend =(uint8_t*) pdu_queue_test.pdu_q[pid_now].pop(&len);   //暂时是前7个进程一直ACK为true,第8个ACK一直为false
        printf("RNTI:%d::: Now PID NO.%d:the retransmission size is %d bytes.\n", rnti, pid_now, len);
        printf("RNTI:%d::: Now retransmission of PID No.%d:queue's No.%d buffer will be sent.\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is());

        return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len);
      }
      else
      {
        retrans_times[pid_now] = 0;
        pdu_queue_test.pdu_q[pid_now].release(); //丢弃超过重发次数的PDU
        printf("RNTI:%d::: The retransmission times overflow!\n", rnti);
        return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len); //返回下一个PDU的指针,超过重发次数,丢弃上一个包,发送下一个包
      }
    }
  }

  bool init(phy_interface_mac *phy_h_, rlc_interface_mac *rlc_, srslte::log *log_h_, bsr_proc *bsr_procedure_, phr_proc *phr_procedure_, pdu_queue::process_callback *callback_)
  {
    //rnti=rnti_;
    pthread_mutex_init(&ACK_LOCK, NULL);
    pthread_mutex_init(&busy_LOCK, NULL);
    pthread_cond_init(&busy_signal, NULL);
    busy_num = 0;
    ue_mux_test.init(rlc_, log_h_, bsr_procedure_, phr_procedure_);
    pdu_queue_test.init(callback_, log_h_);
    mac_demux_test.init(phy_h_, rlc_, log_h_);
  }
  
  uint8_t *harq_busy(uint32_t *pid, uint32_t len) //用于物理层发送，发送一个不出于busy状态的harq进程的pdu
  {
    pthread_mutex_lock(&busy_LOCK);
    while (busy_num >= HARQ_NUM)
    {
      pthread_cond_wait(&busy_signal, &busy_LOCK);
    }
    uint32_t pid_not_busy = 0;
    while (busy_index[pid_not_busy])
    {
      pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
    }
    busy_index[pid_not_busy] = true;
    busy_num++;
    *pid = pid_not_busy;
    pthread_mutex_unlock(&busy_LOCK);
    return trans_control(pid_not_busy, len);
  }
};

class UE_FX //日后用于多用户情况
{
public:
  std::map<uint16_t, UE_process_FX> UE;
};

/*********    RLC测试      **********/
class RLC_interface_FX
{
private:
  //std::map<uint16_t, uint32_t> unread_list;
  uint32_t n_unread;
  uint32_t idx;
  uint16_t rnti[SIZE_RLC_QUEUE];
  rlc_um *rlc;

public:
  void init(rlc_um *rlc_)
  {
    rlc = rlc_;
    idx = 0;
    n_unread = 0;
  }
  bool set(uint16_t i_rnti)
  {
    if (n_unread != SIZE_RLC_QUEUE)
    {
      rnti[idx] = i_rnti;
      idx = (idx + 1) % SIZE_RLC_QUEUE;
      ++n_unread;
      return true;
    }
    else
    {
      return false;
    }
  }
  int had_to_mac()
  {
    if (n_unread > 0)
    {
      return (--n_unread);
    }
  }
};
#endif