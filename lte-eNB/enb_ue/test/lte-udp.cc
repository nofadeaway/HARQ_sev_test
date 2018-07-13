#include "FuncHead.h"

#define SEND_SIZE 400

using namespace srslte;
using namespace srsue;

// struct D_DCI
// {
// 	uint32_t N_pid_now;
// };    //结构体永远别忘了加分号...

extern mux ue_mux_test;
extern srslte::pdu_queue pdu_queue_test; //5.28
//bool ACK[8]={false,false,false,false,false,false,false,false};

extern pthread_barrier_t barrier;

void *lte_send_udp(void *ptr)
{

	int port_add = 0; //FX:7.10
	if (ptr != NULL)
	{
		port_add = *((int *)ptr);
		printf("UDP:send The port offset is %d\n", port_add);
	}
	else
	{
		printf("UDP:No port offset inport.\n");
	}

	//printf("enter--lte_send_udp\n");
	usleep(5000);

	int port = atoi("6604");
	port = port + port_add;
	//create socket
	int st = socket(AF_INET, SOCK_DGRAM, 0); //int socket( int af, int type, int protocol); 使用UDP则第二个参数为（SOCK_DGRAM）
	if (st == -1)
	{
		printf("create socket failed ! error message :%s\n", strerror(errno));
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr("10.129.4.106"); //目的实际地址

	uint8_t *payload_test = new uint8_t[SEND_SIZE];
	uint8_t *payload_back = new uint8_t[SEND_SIZE];

	uint32_t pdu_sz_test = 300; //下面其实应该发送最终打包长度吧,待修改
	uint32_t tx_tti_test = 1;
	uint32_t pid_test = 8; //目前暂时只有1个进程
	uint32_t pid_now = 0;

	//begin{5.29}
	uint8_t *payload_tosend = new uint8_t[SEND_SIZE];
	// bool qbuff_flag=false;   //记录 qbuff::send()返回值
	//end{5.29}

	//7.3begin{发送DCI}
	int port_a = atoi("7707"); //发送DCI端口
	port_a = port_a + port_add;
	//create socket
	int st_a = socket(AF_INET, SOCK_DGRAM, 0);
	if (st_a == -1)
	{
		printf("create socket failed ! error message :%s\n", strerror(errno));
	}

	struct sockaddr_in addr_a;
	memset(&addr_a, 0, sizeof(addr_a));
	addr_a.sin_family = AF_INET;
	addr_a.sin_port = htons(port_a);
	addr_a.sin_addr.s_addr = inet_addr("10.129.4.106"); //目的实际地址
														//7.3end{发送DCI}
														//sleep(1);
	pthread_barrier_wait(&barrier);

	while (1)
	{

		memset(payload_test, 0, SEND_SIZE * sizeof(uint8_t));
		memset(payload_back, 0, SEND_SIZE * sizeof(uint8_t));
		memset(payload_tosend, 0, SEND_SIZE * sizeof(uint8_t)); //FX

		//uint8_t* mux::pdu_get(uint8_t *payload, uint32_t pdu_sz, uint32_t tx_tti, uint32_t pid)

		payload_back = ue_mux_test.pdu_get(payload_test, pdu_sz_test, tx_tti_test, pid_now);
		pdu_store(pid_now, payload_back, pdu_sz_test);
		// printf("Now this pdu belongs to HARQ NO.%d\n",pid_now);

		// //begin{5.28添加}
		// if(pdu_queue_test.request_buffer(pid_now,pdu_sz_test))     //request_buffer函数返回指为qbuff中ptr指针,而在下面send中其实并不需要使用
		// {printf("PID No.%d:queue's buffer request succeeded!\n",pid_now);}

		// qbuff_flag=pdu_queue_test.pdu_q[pid_now].send(payload_back,pdu_sz_test);     //把payload)back存入qbuff
		// if(qbuff_flag){
		// 	printf("Succeed in sending PDU to queue's buffer!\n");
		// }
		// else{
		// 	printf("Fail in sending PDU to queue's buffer!\n");
		// }
		//end{5.28添加}

		/***********************************************
	    *控制重发
	    *************************************************/
		// bool ACK[8]={true,false,true,true,true,true,true,false};    //目前HARQ进程最多8个
		// if(ACK[pid_now])
		// {
		//    payload_tosend = payload_back;
		//    printf("Now PID No.%d:queue's No.%d buffer will be sent.\n",pid_now,pdu_queue_test.pdu_q[pid_now].wp_is());
		// }
		// else
		// {
		//    //memcpy(payload_tosend, pdu_queue_test.pdu_q[pid_now].pop(pdu_sz_test,1), pdu_sz_test);
		//    uint32_t len=pdu_sz_test;
		//    payload_tosend =(uint8_t*) pdu_queue_test.pdu_q[pid_now].pop(&len);   //暂时是前7个进程一直ACK为true,第8个ACK一直为false
		//    printf("Now PID NO.%d:the retransmission size is %d bytes.\n",pid_now,len);
		//    printf("Now PID No.%d:queue's No.%d buffer will be sent.\n",pid_now,pdu_queue_test.pdu_q[pid_now].rp_is()+1);
		// }

		//FX   发送DCI
		char temp_DCI[100];
		D_DCI DCI_0;
		DCI_0.N_pid_now = pid_now;

		memset(temp_DCI, 0, sizeof(temp_DCI));
		memcpy(temp_DCI, &DCI_0, sizeof(D_DCI));
		if (sendto(st_a, temp_DCI, sizeof(DCI_0), 0, (struct sockaddr *)&addr_a, sizeof(addr_a)) == -1)
		{
			printf("DCI:sendto failed ! error message :%s\n", strerror(errno));
		}
		else
		{
			printf("Thread_UDP UE No.%d: UDP trans begin! NO.%d:DCI succeed!\n", port_add, pid_now);
		}
		//end

		/*******************************************/
		//FX：begin{发送udp}
		//添加一个轮询pid_now,如果当前这个pid_now的ACK正在被rece修改，则去取下一个

		//
		payload_tosend = trans_control(pid_now, pdu_sz_test, port_add);
		if (sendto(st, payload_tosend, pdu_sz_test, 0, (struct sockaddr *)&addr,
				   sizeof(addr)) == -1)
		{
			printf("sendto failed ! error message :%s\n", strerror(errno));
			break;
		}

		usleep(200000);
		//FX：end{发送udp}
		/**********************************/

		//师兄的发送部分
		// if (sendto(st, payload_back, pdu_sz_test, 0, (struct sockaddr *) &addr,
		// 	sizeof(addr)) == -1)
		// {
		// 	printf("sendto failed ! error message :%s\n", strerror(errno));
		// 	break;
		// }
		// sleep(1);

		//FX:5.28添加
		pid_now = pid_now + 1; //循环发送8个进程
		if (pid_now == 3)
		{
			pid_now = 0;
		}
		//5.28添加
	}

	delete[] payload_back;
	delete[] payload_test;
	delete[] payload_tosend;

	close(st);

	close(st_a);
}
