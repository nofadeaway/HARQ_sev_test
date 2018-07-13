#ifndef FX_MAC_H
#define FX_MAC_H


#define Error(fmt, ...)   log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <map>

#include "../hdr/common/log.h"
#include "../hdr/mac/mac.h"
#include "../hdr/common/pcap.h"

class mac_interface_phy_FX
{
public:
    
  typedef struct {
    uint32_t    pid;    
    uint32_t    tti;
    uint32_t    last_tti;
    bool        ndi; 
    bool        last_ndi; 
    uint32_t    n_bytes;
    int         rv; 
    uint16_t    rnti; 
    bool        is_from_rar;
    bool        is_sps_release;
    bool        has_cqi_request;
    srslte_rnti_type_t rnti_type; 
    srslte_phy_grant_t phy_grant; 
  } mac_grant_t; 
  
  typedef struct {
    bool                    decode_enabled;
    int                     rv;
    uint16_t                rnti; 
    bool                    generate_ack; 
    bool                    default_ack; 
    // If non-null, called after tb_decoded_ok to determine if ack needs to be sent
    bool                  (*generate_ack_callback)(void*); 
    void                   *generate_ack_callback_arg;
    uint8_t                *payload_ptr; 
    srslte_softbuffer_rx_t *softbuffer;
    srslte_phy_grant_t      phy_grant;
  } tb_action_dl_t;

  typedef struct {
    bool                    tx_enabled;
    bool                    expect_ack;
    uint32_t                rv;
    uint16_t                rnti; 
    uint32_t                current_tx_nb;
    srslte_softbuffer_tx_t *softbuffer;
    srslte_phy_grant_t      phy_grant;
    uint8_t                *payload_ptr; 
  } tb_action_ul_t;

};
class UE_process_FX               //处理之前的程序
{
  public:
   rlc_um rlc3;
   mac_dummy_timers timers_test;
   mux ue_mux_test;
   demux mac_demux_test;
   demux mac_demux_test_trans;		  //用于发送方的，其中自动会有pdu_queue
   srslte::pdu_queue pdu_queue_test; //自己添加的PDU排队缓存,目前支持的HARQ进程数最多为8，既最多缓存8个PDU
   bool ACK[8] = {false, false, false, false, false, false, false, false};
};
class UE_FX
{
   public:
        std::map<uint16_t,UE_process_FX> UE;
};

#endif