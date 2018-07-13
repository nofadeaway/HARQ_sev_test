本层的PDU为下层的SDU；本层的SDU为上层的PDU
SDU服务数据单元，对应于某个子层中没有被处理的数据。对于某个子层而言，进来的是SDU。
PDU协议数据单元，对应于被该子层处理形成特定格式的数据。对于某个子层而言，出去的就是PDU.
 所以说，在MAC层，RLC来的是SDU，PDU是封装后要交给物理层的。


ite-ue-main.cc 注释掉rece

2018.5.17  lte-ue-main.cc 的 phy_interface_mac类，在common头文件中已经被注释掉了

pdu_queue::pdu_queue.h 62行 std::vector<qbuff> pdu_q;    //PDU buffer
class qbuff 定义于qbuff.h

5.21  目前思路，在组包MUX的输出PDU存入PDU_queue,实现PDU_queue和DL_Harq的pid，在UE端做一个回复ACK的程序。
      common 46行 被注释掉了。
      mux.cc  203行uint8_t *ret = pdu_msg.write_packet(log_h);将PDU存入了buffer，涉及了srslte::sch_pdu pdu_msg;
      srslte::sch_pdu 定义于common的266行，该类的具体函数写在pdu.cc中

      namespace srslte {

class pdu_queue
{
public:
  class process_callback
  {
    public: 
      virtual void process_pdu(uint8_t *buff, uint32_t len) = 0;
  };  //这个纯虚函数是干啥的...
  5.22 修改了 pdu_queue.h 为:
  public: 
      //virtual void process_pdu(uint8_t *buff, uint32_t len) = 0;
      std::vector<bool> pro_callback(NOF_HARQ_PID,,false);     //自己加的用于ACK
  };

  修改了ite-udp

  修改了pdu_queue.cc和pdu_queue.h

  5.28
 探究为什么pdu_queue被初始化了2次
 注释掉了原程序中的pdu_queue 仍然发现pdu_queue初始化了，说明原程序已经调用了这个
 注释掉了main中的 mux初始化，仍然有pdu_queue初始化
   注释掉了demux初始化后，pdu_queue初始化信息消失，说明demux中使用到了pdu_queue
   demux.init()函数中包含了pdu.init(),其中pdu为srslte::pdu_queue pdus，所以此处调用了pdu_queue的init函数;

mux中的PDU缓冲结构为：    写在定义于pdu.h
     /* PDU Buffer */
  srslte::sch_pdu    pdu_msg; 
  bool msg3_has_been_transmitted;
  
   改动：在main中加了demux mac_demux_test_trans;来初始化一个pdu_queue
        目前看到了 q_buff的push



5.29 
qbuff::init   srslte_vec_malloc 中 这个if里调用了posix_memalign(&ptr,32,size)已经申请成功了，所以再return
qbuff::send 函数是真正的将数据存入的函数，其中还调用了qbuff::push函数

5.30
给qbuff结构体写入了取出rp和wp的函数

6.30
当前问题：加入最大重传次数，设置超时。  超时可以用发送来设置，最多等几个包。
        考虑设置怎样的结构。

7.31
//int socket( int af, int type, int protocol); UDP（SOCK_DGRAM） 

修改了 FuncHead.c   添加了ip-trans-data函数

7.2
将UE端程序中 main函数中的 lte-udp线程去注释掉了
修改UE端的receive程序，开始以结构体形式，发送ACK   端口5500
修改了EnB端的receive程序，加入了接受端口5500发来的ACK

问题： bind函数无法绑定ip，原因：ip地址不能为其他机器的地址。
send()函数只能用于连接已经建立的情况，未建立连接时用sendto()

7.3在EnB端加了的lte-udp程序多加了一个socket 用一个端口 发送 伪的DCI 里面 其中之包涵了 harq进程号这一个信息

目前端口号   数据6604   ACK5500 DCI 7707


7.4 注释掉了 rlc_um.cc 中所有 log->info
    注释掉了 mux中各色printf debug
    注释掉了 demux中各色info debug

.7.5 
    在mux.inti（）里添加了一句 pthread_mutex_init(&mutex, NULL); 又改回去了

7.6 
    phr_procedure->generate_phr_on_ul_grant(&phr_value)  溢出为调用了此函数，在pdu_get()中，代码位于mux.cc

    mac.h 中的mac类包含了全部的这些 各色全都有

7.10
    linux定时 timerfd
    添加了 pthread_barrier_t barrier; //于main函数
    VS Code 代码快速格式化快捷键:     windows：Shift + Alt + F    Mac：Shift + Option + F   Ubuntu： Ctrl + Shift + I   

    问题：多线程中，共用了同一个ACK数组 和 同一个 queue类中的队列，这显然不行。
    计划：在每个lte-udp进程中单独才创建一个queue类队列，或者在lte-ue-main中创建   port_add 正好指明 下标

7.11
    phch_worker.cc: phy->mac->tb_decoded(dl_ack, dl_mac_grant.rnti_type, dl_mac_grant.pid);
    其中 phy 为 class phch_worker 中phch_common    *phy;