struct engine{
	engine_head;
	int32_t kfd;
	struct kevent* events;
	int    maxevents;
	int    tfd;
	//for timer
   	struct kevent change;	
};

int32_t event_add(engine *e,handle *h,int32_t events)
{	
	struct kevent ke;
	EV_SET(&ke, h->fd, events, EV_ADD, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	h->e = e;
	dlist_pushback(&e->handles,(dlistnode*)h);
	return 0;	
}

int32_t event_remove(handle *h)
{
	struct kevent ke;
	engine *e = h->e;
	EV_SET(&ke, h->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, h->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(e->kfd, &ke, 1, NULL, 0, NULL);
	h->events = 0;
	h->e = NULL;
	dlist_remove(cast(dlistnode*,h));	
	return 0;	
}

int32_t event_enable(handle *h,int32_t events)
{
	struct kevent ke;
	engine *e = h->e;
	EV_SET(&ke, h->fd, events,EV_ENABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;
	if(events == EVFILT_READ)
		h->set_read = 1;
	else if(events == EVFILT_WRITE)
		h->set_write = 1;
	return 0;
}

int32_t event_disable(handle *h,int32_t events)
{
	struct kevent ke;
	engine *e = h->e;
	EV_SET(&ke, h->fd, events,EV_DISABLE, 0, 0, h);
	errno = 0;
	if(0 != kevent(e->kfd, &ke, 1, NULL, 0, NULL))
		return -errno;

	if(events == EVFILT_READ)
		h->set_read = 0;
	else if(events == EVFILT_WRITE)
		h->set_write = 0;
	
	return 0;
}


void   engine_init_timer(engine *e){
	if(!e->e->timermgr){
		e->tfd      = 1;
		e->timermgr = wheelmgr_new();
		EV_SET(&e->change, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, 1, e->timermgr);
	}
}

static int32_t engine_init(engine *e)
{
	int32_t kfd = kqueue();
	if(kfd < 0) return NULL;
	fcntl (kfd, F_SETFD, FD_CLOEXEC);
	int32_t tmp[2];
	if(pipe2(tmp,O_NONBLOCK|O_CLOEXEC) != 0){
		close(kfd);
		return NULL;
	}		

	e->kfd = kfd;
	e->maxevents = 64;
	e->events = calloc(1,(sizeof(*e->events)*e->maxevents));
	e->notifyfds[0] = tmp[0];
	e->notifyfds[1] = tmp[1];
	struct kevent ke;
	EV_SET(&ke,tmp[0], EVFILT_READ, EV_ADD, 0, 0, tmp[0]);
	errno = 0;
	if(0 != kevent(kfd, &ke, 1, NULL, 0, NULL)){
		close(kfd);
		close(tmp[0]);
		close(tmp[1]);
		free(e->events);
		free(e);
		return -1;	
	}
	e->threadid = thread_id();
	dlist_init(&e->handles);			
	return 0;
}

static inline void _engine_del(engine *e)
{
	handle *h;
	if(e->tfd){
		wheelmgr_del(e->timermgr);
	}
	while((h = cast(handle*,dlist_pop(&e->handles)))){
		event_remove(h);
		h->on_events(h,EVENT_ECLOSE);
	}
	e->tfd = 0;	
	close(e->notifyfds[0]);
	close(e->notifyfds[1]);
	free(e->events);
}

int32_t _engine_run(engine *e,int32_t timeout)
{
	int32_t ret = 0;
	errno = 0;
	int32_t i,nfds;
	handle *h;
	struct timespec ts;
	uint64_t msec;
	if(timeout >= 0){
		msec = timeout%1000;
		ts.tv_nsec = (msec*1000*1000);
		ts.tv_sec   = (timeout/1000);
	}		
	do{
		nfds = TEMP_FAILURE_RETRY(kevent(e->kfd, &e->change,e->tfd? 1 : 0, e->events,e->maxevents, timeout < 0 ?NULL:&ts));	
		if(nfds > 0){
			e->status |= INLOOP;
			for(i=0; i < nfds ; ++i)
			{
				struct kevent *event = &e->events[i];
				if(event->udata == e->notifyfds[0]){
					int32_t _;
					while(TEMP_FAILURE_RETRY(read(e->notifyfds[0],&_,sizeof(_))) > 0);
					goto loopend;
				}else if(event->udata == e->timermgr){
					wheelmgr_tick(e->timermgr,systick64());
				}else{
					h = cast(handle*,event->udata);
					h->on_events(h,event->filter);;
				}
			}
			e->status ^= INLOOP;
			if(e->status & CLOSING)
				break;			
			if(nfds == e->maxevents){
				free(e->events);
				e->maxevents <<= 2;
				e->events = calloc(1,sizeof(*e->events)*e->maxevents);
			}				
		}else if(nfds < 0){
			ret = -errno;
			break;
		}	
	}while(timeout < 0);
loopend:	
	if(e->status & CLOSING){
		ret = -EENGCLOSE;
#ifdef _CHUCKLUA
		engine_del_lua(e);
#else
		engine_del(e);
#endif
	}	
	return ret;
}