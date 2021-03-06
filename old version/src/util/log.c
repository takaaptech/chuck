#include <string.h>
#include "util/log.h"
#include "util/list.h"
#include "util/dlist.h"
#include "thread/thread.h"
#include "util/atomic.h"
#include "comm.h"

static pthread_once_t 	g_log_key_once = PTHREAD_ONCE_INIT;
static dlist            g_log_file_list = {};
static thread_t    	    g_log_thd;
static pid_t          	g_pid = -1;
static uint32_t         flush_interval = 1;  //flush every 1 second
static volatile int32_t stop = 0;
static int32_t   		lock = 0;
int32_t                 g_loglev = LOG_INFO;

#define LOCK() while (__sync_lock_test_and_set(&lock,1)) {}
#define UNLOCK() __sync_lock_release(&lock);


const char *log_lev_str[] = {
	"INFO",
	"DEBUG",
	"WARN",
	"ERROR",
	"CRITICAL",
};

enum{
	NONE = 0,
	CHANGE  = 1 << 1,
	CLOSING = 1 << 2
};

typedef struct logfile{
	dlistnode  node;
	volatile uint32_t refcount;
	char       filename[256];
	FILE      *file;
	uint32_t   total_size;
	int8_t     status;
}logfile;

struct log_item{
	listnode node;
	int32_t  loglev;
	logfile *_logfile;
	char     content[0];
};

struct{
	mutex       *mtx;
	condition   *cond;
	list         private_queue;
	list         share_queue;
	volatile int wait;	
}logqueue;


static void  write_console(int32_t loglev,char *content) {
	switch(loglev) {
		case LOG_INFO     : printf("%s\n",content); break;
		case LOG_DEBUG    : printf("\033[1;32;40m%s\033[0m\n",content); break;
		case LOG_WARN     : printf("\033[1;33;40m%s\033[0m\n",content); break;
		case LOG_ERROR    : printf("\033[1;35;40m%s\033[0m\n",content); break;
		case LOG_CRITICAL : printf("\033[5;31;40m%s\033[0m\n",content); break;
		default           : break;		
 	}
}

void logqueue_push(struct log_item *item)
{
	write_console(item->loglev,item->content);
	mutex_lock(logqueue.mtx);
	list_pushback(&logqueue.share_queue,(listnode*)item);
	if(logqueue.wait && list_size(&logqueue.share_queue) == 1){
		mutex_unlock(logqueue.mtx);
		condition_signal(logqueue.cond);
		return;
	}
	mutex_unlock(logqueue.mtx);
}

struct log_item *logqueue_fetch(uint32_t ms)
{
	if(list_size(&logqueue.private_queue) > 0)
		return cast(struct log_item*,list_pop(&logqueue.private_queue));
	mutex_lock(logqueue.mtx);
	if(ms > 0){
		if(list_size(&logqueue.share_queue) == 0){
			logqueue.wait = 1;
			condition_timedwait(logqueue.cond,ms);
			logqueue.wait = 0;
		}
	}
	if(list_size(&logqueue.share_queue) > 0){
		list_pushlist(&logqueue.private_queue,&logqueue.share_queue);
	}
	mutex_unlock(logqueue.mtx);
	return cast(struct log_item*,list_pop(&logqueue.private_queue));
}


DEF_LOG(sys_log,SYSLOG_NAME);
IMP_LOG(sys_log);

int32_t write_prefix(char *buf,int32_t loglev)
{
	struct timespec tv;
	struct tm _tm;
    clock_gettime (CLOCK_REALTIME, &tv);	
	localtime_r(&tv.tv_sec, &_tm);
	return sprintf(buf,"[%s]%04d-%02d-%02d-%02d:%02d:%02d.%03d[%u]:",
				   log_lev_str[loglev],
				   _tm.tm_year+1900,
				   _tm.tm_mon+1,
				   _tm.tm_mday,
				   _tm.tm_hour,
				   _tm.tm_min,
				   _tm.tm_sec,
				   cast(int32_t,tv.tv_nsec/1000000),
				   cast(uint32_t,thread_id()));
}

static void *log_routine(void *arg)
{
	time_t next_fulsh = time(NULL) + flush_interval;
	struct log_item *item;
	char   filename[128],buf[128];
	struct timespec tv;
	struct tm _tm;
	struct logfile *l;
	int32_t size;
	dlistnode *n;			
	while(1){
		item = logqueue_fetch(stop?0:100);
		if(item){
			if(item->_logfile->status & CLOSING && 0 == strcmp(item->content,"close")){
				if(item->_logfile->file){
					size = write_prefix(buf,LOG_INFO);
					snprintf(&buf[size],128-size,"log close success");
					fprintf(item->_logfile->file,"%s\n",buf);
					fflush(item->_logfile->file);
					fclose(item->_logfile->file);					
				}
				free(item->_logfile);
				item->_logfile = NULL; 
			}else if(item->_logfile->file == NULL || item->_logfile->total_size > MAX_FILE_SIZE)
			{
				if(item->_logfile->total_size){
					fclose(item->_logfile->file);
					item->_logfile->total_size = 0;
				}
				//还没创建文件
				clock_gettime(CLOCK_REALTIME, &tv);
				localtime_r(&tv.tv_sec, &_tm);
				snprintf(filename,128,"%s[%d]-%04d-%02d-%02d %02d.%02d.%02d.%03d.log",
						 item->_logfile->filename,
						 getpid(),
					     _tm.tm_year+1900,
					     _tm.tm_mon+1,
					     _tm.tm_mday,
					     _tm.tm_hour,
					     _tm.tm_min,
					     _tm.tm_sec,
					     cast(int32_t,tv.tv_nsec/1000000));
				item->_logfile->file = fopen(filename,"w+");
			}
			if(item->_logfile && item->_logfile->file){
				fprintf(item->_logfile->file,"%s\n",item->content);
				item->_logfile->total_size += strlen(item->content);
				item->_logfile->status |= CHANGE;
			}
			free(item);			
		}
		else if(stop) break;
		if(time(NULL) >= next_fulsh){
			l = NULL;	
			LOCK();
			dlist_foreach(&g_log_file_list,n){
				l = cast(struct logfile *,n);
				if(l->file && l->status & CHANGE){
					fflush(l->file);
					l->status ^= CHANGE;
				}
			}
			UNLOCK();
			next_fulsh = time(NULL) + flush_interval;
		}
	}

	//向所有打开的日志文件写入"log close success"
	LOCK();
	dlist_foreach(&g_log_file_list,n){
		l = cast(struct logfile*,n);
		if(l->file){
			size = write_prefix(buf,LOG_INFO);
			snprintf(&buf[size],128-size,"log close success");
			fprintf(l->file,"%s\n",buf);
			fflush(l->file);
			fclose(l->file);			
		}
	}	
	UNLOCK();
	return NULL;
}

static void on_process_end()
{
	if(g_pid == getpid()){
		stop = 1;
		thread_join(g_log_thd);
	}
}

void _write_log(logfile *l,int32_t loglev,const char *content)
{
	uint32_t content_len = strlen(content)+1;
	struct log_item *item = calloc(1,sizeof(*item) + content_len);
	item->_logfile = l;
	item->loglev   = loglev;
	strncpy(item->content,content,content_len);	
	logqueue_push(item);
}
			           
static void log_once_routine()
{
	g_pid = getpid();	
	dlist_init(&g_log_file_list);
	list_init(&logqueue.private_queue);
	list_init(&logqueue.share_queue);
	logqueue.mtx = mutex_new();
	logqueue.cond = condition_new(logqueue.mtx);
	g_log_thd = thread_new(log_routine,NULL);	
	atexit(on_process_end);
}

logfile *create_logfile(const char *filename)
{
	logfile *l;
	pthread_once(&g_log_key_once,log_once_routine);
	l = calloc(1,sizeof(*l));
	strncpy(l->filename,filename,256);
	LOCK();
	dlist_pushback(&g_log_file_list,cast(dlistnode*,l));
	UNLOCK();	
	return l;
}

void write_log(logfile* l,int32_t loglev,const char *content)
{
	_write_log(l,loglev,content);
}

void write_sys_log(int32_t loglev,const char *content)
{
	_write_log(GET_LOGFILE(sys_log),loglev,content);
}


#ifdef _CHUCKLUA

#include "lua/lua_util.h"
#include "util/refobj.h"

#define LUA_METATABLE "log_meta"

struct lua_logfile{
	logfile *l;
};

static void close_logfile(logfile *l)
{
	struct log_item *item;
	LOCK();
	if(!(l->status & CLOSING)){
		l->status |= CLOSING;
		dlist_remove(cast(dlistnode*,l));
		item = calloc(1,sizeof(*item) + strlen("close") + 1);
		item->_logfile = l;
		strcpy(item->content,"close");	
		logqueue_push(item);		
	}	
	UNLOCK();
} 

static int32_t lua_create_logfile(lua_State *L)
{
	logfile    *l = NULL;
	dlistnode  *n;
	const char *filename;
	struct lua_logfile *ll;
	pthread_once(&g_log_key_once,log_once_routine);
	if(lua_isstring(L,1)){
		filename = lua_tostring(L,1);
		LOCK();
		n = dlist_begin(&g_log_file_list);
		while(n != dlist_end(&g_log_file_list)){
			if(strcmp(cast(logfile*,n)->filename,filename) == 0){
				l = cast(logfile*,n);
				ATOMIC_INCREASE_FETCH(&l->refcount);
			}
			n = n->next;
		}	
		UNLOCK();
		if(!l){
			l = create_logfile(filename);
			if(!l) return 0;
			ATOMIC_INCREASE_FETCH(&l->refcount);
		}
		ll = cast(struct lua_logfile*,lua_newuserdata(L, sizeof(*ll)));
		ll->l = l;
		luaL_getmetatable(L, LUA_METATABLE);
		lua_setmetatable(L, -2);
		return 1;	
	}
	return 0;	
}

static int32_t lua_syslog(lua_State *L)
{
	int32_t loglev;
	const   char *msg;
	if(lua_isnumber(L,1) && lua_isstring(L,2)){
		loglev = cast(int32_t,lua_tointeger(L,1));
		msg = lua_tostring(L,2);
		SYS_LOG(loglev,"%s",msg);
	}
	return 0;
}

struct lua_logfile *to_lua_logfile(lua_State *L, int index) 
{
	return cast(struct lua_logfile*,luaL_testudata(L, index, LUA_METATABLE));
}

static int32_t lua_logfile_gc(lua_State *L)
{
	struct lua_logfile *ll = to_lua_logfile(L,1);
	if(ATOMIC_DECREASE_FETCH(&ll->l->refcount) == 0)
		close_logfile(ll->l);
	return 0;
}

static int32_t lua_write_log(lua_State *L)
{
	int32_t loglev;
	const   char *msg;
	struct lua_logfile *ll = to_lua_logfile(L,1);
	if(!ll) return 0;
	if(lua_isnumber(L,2) && lua_isstring(L,3)){
		loglev = cast(int32_t,lua_tointeger(L,2));
		msg = lua_tostring(L,3);
		LOG(ll->l,loglev,"%s",msg);
	}
	return 0;	
}

static int32_t lua_set_loglev(lua_State *L)
{
	if(lua_isnumber(L,1))
		set_log_lev(lua_tointeger(L,1));
	return 0;
}

#define SET_FUNCTION(L,NAME,FUNC) do{\
	lua_pushstring(L,NAME);\
	lua_pushcfunction(L,FUNC);\
	lua_settable(L, -3);\
}while(0)

#define SET_CONST(L,C,N) do{\
		lua_pushstring(L, N);\
		lua_pushinteger(L,C);\
		lua_settable(L, -3);\
}while(0)

void lua_reglog(lua_State *L)
{
    luaL_Reg log_mt[] = {
        {"__gc", lua_logfile_gc},
        {NULL, NULL}
    };

    luaL_Reg log_methods[] = {
        {"Log",lua_write_log},
        {NULL,     NULL}
    };

    luaL_newmetatable(L, LUA_METATABLE);
    luaL_setfuncs(L, log_mt, 0);

    luaL_newlib(L, log_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
 
    lua_newtable(L);
    SET_CONST(L,LOG_INFO,"INFO");
    SET_CONST(L,LOG_DEBUG,"DEBUG");
    SET_CONST(L,LOG_WARN,"WARN");
    SET_CONST(L,LOG_ERROR,"ERROR");
    SET_CONST(L,LOG_CRITICAL,"CRITICAL");
    SET_FUNCTION(L,"SetLogLev",lua_set_loglev);
    SET_FUNCTION(L,"SysLog",lua_syslog);
    SET_FUNCTION(L,"LogFile",lua_create_logfile);
}

#endif
