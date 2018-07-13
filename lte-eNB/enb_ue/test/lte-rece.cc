#include "FuncHead.h"

#define PAYLOAD_SIZE 400

using namespace srslte;
using namespace srsue;

extern demux mac_demux_test;
extern mac_dummy_timers timers_test;
//extern bool ACK[8];
extern bool ACK[];

extern eNB_ACK I_ACK[];

// struct A_ACK
// {
// 	uint32_t ACK_pid;
// 	bool ack_0;
// };

pthread_mutex_t ACK_LOCK = PTHREAD_MUTEX_INITIALIZER;
extern pthread_barrier_t barrier;
/**************************************************************************
* ipsend:从tun中读数据并压入队列
**************************************************************************/

void *lte_rece(void *ptr)
{

	int port_add = 0;
	if (ptr != NULL)
	{
		port_add = *((int *)ptr);
		printf("Recv:send The port offset is %d\n", port_add);
	}
	else
	{
		printf("Recv:No port offset inport.\n");
	}

	//printf("enter--lte_rece\n");

	int st = socket(AF_INET, SOCK_DGRAM, 0);
	if (st == -1)
	{
		printf("open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port = atoi("8808"); //接受数据的端口
    port = port +port_add;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(st, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}

	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);

	int rece_size = 300, k = 0;
	; //修改为随机啊！！！！！！！！！！！
	uint8_t rece_payload[1000][PAYLOAD_SIZE] = {0};

	/****************************/
	//ACK接受
	int st_a = socket(AF_INET, SOCK_DGRAM, 0);
	if (st_a == -1)
	{
		printf("ACK:open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port_a = atoi("5500");
	port_a = port_a + port_add;

	struct sockaddr_in addr_a;

	addr_a.sin_family = AF_INET;
	addr_a.sin_port = htons(port_a);
	addr_a.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(st_a, (struct sockaddr *)&addr_a, sizeof(addr_a)) == -1)
	{
		printf("ACK_receive:bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}

	/****************************/
	pthread_barrier_wait(&barrier);

	while (1)
	{

		if (k == 1000)
		{
			k = 0;
		}

		//作用把内存清零
		// memset(&client_addr, 0, sizeof(client_addr));   //void *memset(void *s, int ch, size_t n);将s中当前位置后面的n个字节 （typedef unsigned int size_t ）用 ch 替换并返回 s

		// if (recvfrom(st, rece_payload[k], rece_size, 0, (struct sockaddr *)&client_addr, &addrlen) == -1) {

		// 	printf("recvfrom failed ! error message : %s\n", strerror(errno));
		// 	goto END;
		// }
		// else {
		// 	//MAC->RLC->IP 第二个参数有误,先固定与接收端一致,但是貌似不影响解包,丢弃了
		// 	mac_demux_test.process_pdu(rece_payload[k], rece_size);
		// 	while(!timers_test.get(-1)->is_expired()){ timers_test.get(-1)->step();}
		// }

		//FX   接受ACK
		char temp[100];
		A_ACK ack_reply;
		//ack_reply.ack_0=true;
		memset(temp, 0, sizeof(temp));

		if (recv(st_a, temp, sizeof(ack_reply), 0) == -1)
		{
			printf("ACK:recvfrom failed ! error message : %s\n", strerror(errno));
		}
		else
		{
			memcpy(&ack_reply, temp, sizeof(ack_reply));
			if (pthread_mutex_lock(&ACK_LOCK) != 0)
			{
				printf("RECE:Lock failed!\n");
			}
			ACK[ack_reply.ACK_pid] = ack_reply.ack_0;
			pthread_mutex_unlock(&ACK_LOCK);
			char str1[10] = "true", str2[10] = "false";
			printf("/******lte-Recv:");
			printf("Thread_RECV No.%d: No.%d ACK received is %s\n",port_add, ack_reply.ACK_pid, (ack_reply.ack_0) ? str1 : str2);
			// if(ack_reply.ack_0==true)
			// {
			// 	ACK[0]=ack_reply.ack_0;
			// 	ACK[1]=ack_reply.ack_0;
			// }
		}
		//
		k++;
	}

END:
	close(st);
ENDD:
	close(st_a);
}
