#ifndef FUNCTIONHEADS_H
#define FUNCTIONHEADS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <iostream>
#include <pthread.h>
#include <time.h>
#include <unistd.h>//sleep
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>

#include "../hdr/common/log_stdout.h"
#include "../hdr/common/interfaces.h"
#include "../hdr/upper/rlc_um.h"
#include "../hdr/mac/mux.h"
#include "../hdr/mac/demux.h"

#include "../hdr/common/pdu_queue.h"
#include "../hdr/common/qbuff.h"

class mac_dummy_timers
	:public srslte::mac_interface_timers
{
public:
	srslte::timers::timer* get(uint32_t timer_id)
	{
		return &t;
	}
	uint32_t get_unique_id() { return 0; }
	void step()
	{
		t.step();
	}

private:
	srslte::timers::timer t;
};
void* lte_send_ip_3(void *ptr);
void* lte_rece(void *ptr);
void* lte_send_udp(void *ptr);
int cwrite(int, uint8_t *, int);
int tun_alloc(char *, int);
#endif
