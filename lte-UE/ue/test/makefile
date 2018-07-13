#######主测试######
OBJS = lte-ue-main.o lte-ip-pkt.o lte-udp.o lte-rece.o lte-tun.o mux.o demux.o pdu.o pdu_queue.o rlc_um.o  buffer_pool.o log_stdout.o qbuff.o vector.o bit.o proc_phr.o proc_bsr.o phy_common.o sequence.o
     
DEST = main
LIBS = -lm -lpthread -lboost_system -lboost_filesystem -lboost_thread -lboost_date_time 

CPPFLAGS = -g -O3 -Wall -I ../hdr -I ../src/commom -I ../../liblte/hdr
           
main: ${OBJS}
	g++ -o ${DEST} ${OBJS} ${LIBS}
clean:
	rm -f ${DEST} ${OBJS}
