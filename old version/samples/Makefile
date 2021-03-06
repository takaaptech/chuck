CFLAGS = -g -Wall -fno-strict-aliasing 
LDFLAGS = -lpthread -lrt -llua -lm -ljemalloc ./../deps/http-parser/libhttp_parser.a
DEPDIR  = 
INCLUDE = -I../src -I./../deps
DEFINE  =

# Platform-specific overrides
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),Linux)
	LDFLAGS += -ldl
	DEFINE += -D_LINUX
endif

ifeq ($(uname_S),FreeBSD)
	LDFLAGS += -lexecinfo
	DEFINE += -D_BSD
endif

source =  stream/connection/conn_echo_server.c\
		  stream/connection/conn_broad_server.c\
		  stream/connection/conn_broad_client.c\
		  stream/tcp_echoserver.c\
		  stream/tcp_echoclient.c\
		  datagram/udp_echoserver.c\
		  datagram/udp_echoclient.c\
		  datagram/datagram/dgram_raw_server.c\
		  datagram/datagram/dgram_raw_client.c\
		  datagram/datagram/dgram_rpk_server.c\
		  datagram/datagram/dgram_rpk_client.c\
		  redis/redisclient.c\
		  redis/redis_stress.c\
		  sock4/proxyserver.c\
		  signal.c\
		  testtimer.c

all:
	@if [ ! -d "bin" ]; then mkdir bin; fi	
	$(CC) $(CFLAGS) -c $(source) $(INCLUDE) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/conn_echo_server conn_echo_server.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/conn_broad_server conn_broad_server.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/conn_broad_client conn_broad_client.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/dgram_raw_server dgram_raw_server.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/dgram_raw_client dgram_raw_client.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/dgram_rpk_server dgram_rpk_server.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/dgram_rpk_client dgram_rpk_client.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/tcp_echoserver tcp_echoserver.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/tcp_echoclient tcp_echoclient.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/udp_echoserver udp_echoserver.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/udp_echoclient udp_echoclient.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/redisclient redisclient.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/redis_stress redis_stress.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/signal signal.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	$(CC) $(CFLAGS) -o bin/proxyserver proxyserver.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)	
	$(CC) $(CFLAGS) -o bin/testtimer testtimer.o ../src/libchuck.a $(INCLUDE) $(LDFLAGS) $(DEFINE)
	rm *.o		  