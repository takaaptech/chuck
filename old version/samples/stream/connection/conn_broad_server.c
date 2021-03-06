#include "chuck.h"
#include "packet/wpacket.h"
#include "packet/rpacket.h"


connection *clients[1000] ={0};

int        client_count = 0;
uint32_t   packet_count = 0;

int32_t timer_callback(uint32_t event,uint64_t _,void *ud){
	if(event == TEVENT_TIMEOUT){
		printf("client_count:%d,packet_count:%u/s\n",client_count,packet_count);
		packet_count = 0;
	}
	return 0;
}

static void on_packet(connection *c,packet *p,int32_t error){
	if(p){
		int i = 0;
		for(;i < client_count; ++i){
			connection *conn = clients[i];
			if(conn){
				packet_count++;
				connection_send(conn,make_writepacket(p),NULL);
			}
		}
	}else{
		//error or peer close
		int32_t i = 0;
		for(;i < client_count; ++i)
			if(clients[i]) clients[i] = NULL;	
		--client_count;
		connection_close(c);		
	}
}

static void on_connection(acceptor *a,int32_t fd,sockaddr_ *_,void *ud,int32_t err){
	if(err == EENGCLOSE){
		acceptor_del(a);
		return;
	}		
	printf("on_connection\n");
	engine *e = (engine*)ud;
	connection *c = connection_new(fd,65535,rpacket_decoder_new(1024));
	engine_associate(e,c,on_packet);

	int i = 0;
	for(;i < 1000; ++i)
		if(!clients[i]){
			clients[i] = c;
			break;
		}
	++client_count;	
}


int main(int argc,char **argv){
	signal(SIGPIPE,SIG_IGN);
	engine *e = engine_new();
	sockaddr_ server;
	if(0 != easy_sockaddr_ip4(&server,argv[1],atoi(argv[2]))){
		printf("invaild address:%s\n",argv[1]);
	}
	int32_t fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	easy_addr_reuse(fd,1);
	if(0 == easy_listen(fd,&server)){
		acceptor *a = acceptor_new(fd,e);
		engine_associate(e,a,on_connection);
		engine_regtimer(e,1000,timer_callback,NULL);
		engine_run(e);
	}else{
		close(fd);
		printf("server start error\n");
	}
	return 0;
}



