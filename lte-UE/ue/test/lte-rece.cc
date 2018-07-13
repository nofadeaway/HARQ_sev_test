#include "FuncHead.h"

#define PAYLOAD_SIZE 400

using namespace srslte;
using namespace srsue;
 
extern demux mac_demux_test;
extern mac_dummy_timers timers_test;
extern pthread_barrier_t barrier;
//用于发送ACK
struct A_ACK
{
	uint32_t ACK_pid;
	bool ack_0;
};
struct D_DCI
{
	uint32_t N_pid_now;
};
/**************************************************************************
* ipsend:从tun中读数据并压入队列
**************************************************************************/

void* lte_rece(void *ptr) {

    
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
	if (st == -1) {
		printf("open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port = atoi("6604");
	port = port + port_add;
	 
	struct sockaddr_in addr;
	 
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	 
	if (bind(st, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		printf("bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
 
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
 
	int rece_size = 300, k=0;;//修改为随机啊！！！！！！！！！！！
	uint8_t rece_payload[1000][PAYLOAD_SIZE] ={0};
    /**********************************/
	//接受DCI的socket
	int st_DCI = socket(AF_INET, SOCK_DGRAM, 0);
	if (st_DCI == -1) {
		printf("DCI:open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port_DCI = atoi("7707");
	port_DCI = port_DCI + port_add;
	 
	struct sockaddr_in addr_DCI;
	socklen_t addrlen_DCI = sizeof(addr_DCI);
	memset(&addr_DCI,0,sizeof(addr_DCI));
	addr_DCI.sin_family = AF_INET;
	addr_DCI.sin_port = htons(port_DCI);
	addr_DCI.sin_addr.s_addr = htonl(INADDR_ANY);
    
	if (bind(st_DCI, (struct sockaddr *)&addr_DCI, sizeof(addr_DCI)) == -1) {
		printf("bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
    
    struct sockaddr_in addr_DCI_1;
	socklen_t addrlen_DCI_1 = sizeof(addr_DCI_1);

	/*********************************/
    //begin{FX:发送ACK}
	int port_a = atoi("5500");    //发送ACK端口
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
	addr_a.sin_addr.s_addr = inet_addr("10.129.4.105");//目的实际地址

	// if (bind(st_a, (struct sockaddr *)&addr_a, sizeof(addr_a)) == -1) {
	// 	printf("ACK:bind IP failed ! error message : %s\n", strerror(errno));
	// 	exit(1);
	// }
	/********************************/
	pthread_barrier_wait(&barrier);
	 
	while (1) {

		if(k == 1000){
			k = 0;
		} 
		 
		//作用把内存清零
		memset(&client_addr, 0, sizeof(client_addr));   //void *memset(void *s, int ch, size_t n);将s中当前位置后面的n个字节 （typedef unsigned int size_t ）用 ch 替换并返回 s 
        memset(&addr_DCI_1, 0, sizeof(addr_DCI_1));   
        //接受DCI
		char temp_DCI[100];
		D_DCI dci;
		memset(temp_DCI,0,sizeof(temp_DCI));
        if (recvfrom(st_DCI, temp_DCI, sizeof(D_DCI), 0, (struct sockaddr *)&addr_DCI_1, &addrlen_DCI_1) == -1) {

			printf("Thread No.%d: UE No.%d: DCI:recvfrom failed ! error message : %s\n",port_add,port_add, strerror(errno));
			
		}
		else{
			memcpy(&dci,temp_DCI,sizeof(D_DCI));
			printf("**************************************************\n");
			printf("Thread No.%d: UE No.%d:DCI recv succeed!The next PDU belongs to NO.%d.\n",port_add,port_add,dci.N_pid_now);
		}

		if (recvfrom(st, rece_payload[k], rece_size, 0, (struct sockaddr *)&client_addr, &addrlen) == -1) {

			printf("recvfrom failed ! error message : %s\n", strerror(errno));
			goto END;
		}
		else {
			//MAC->RLC->IP 第二个参数有误,先固定与接收端一致,但是貌似不影响解包,丢弃了
			mac_demux_test.process_pdu(rece_payload[k], rece_size);
            printf("Thread No.%d: lte-Rece:PDU received!\n",port_add);
			//FX   发送ACK
		   char temp[100];
		   A_ACK ack_reply;
		   ack_reply.ACK_pid=dci.N_pid_now;
           ack_reply.ack_0=true;
		   if(k%3==0)
		   {  ack_reply.ack_0=false; }
		   memset(temp,0,sizeof(temp));
		   memcpy(temp,&ack_reply,sizeof(ack_reply));
           if(sendto(st_a,temp,sizeof(ack_reply),0,(struct sockaddr *) &addr_a,sizeof(addr_a))==-1)
		   {printf("Thread No.%d: UE No.%d: ACK:sendto failed ! error message :%s\n",port_add,port_add, strerror(errno));}
		   else
		   {
			   printf("(Thread No.%d: UE No.%d): NO.%d:ACK sending succeed!\n",port_add,port_add,ack_reply.ACK_pid);
			   printf("************************************\n");
		   }
		   //end

			while(!timers_test.get(-1)->is_expired()){ timers_test.get(-1)->step();}		
		}
		
		k++;
	}

	END:close(st);
    close(st_a);
}
