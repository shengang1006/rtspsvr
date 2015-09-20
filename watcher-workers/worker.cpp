
#include <arpa/inet.h>  
#include "worker.h"

worker::worker(){
	m_listenfd = -1;
	m_epfd     = -1;
	m_keepalive  = 120;
	m_brun = false;
	m_event_list = NULL;
	m_event_num = 0;
}

worker::~worker(){
	release();
}


int worker::run(){

	warn_log("thread(%u) start\n", pthread_self());

	while(m_brun){
			
		//epoll wait
		int res = epoll_wait(m_epfd, m_event_list, m_event_num, m_timer.latency_time());
		if(res == -1){
			error_log("epoll_wait error(%d %s)\n", errno, strerror(errno));
			if (errno != EINTR){
				break;
			}
			else{
				continue;
			}
		}

		//handle epoll event
		for(int i = 0; i < res; i++){
			//accept, bug here
			if (m_event_list[i].data.ptr == &m_listenfd){
				handle_accept();
				continue;
			}
			
			connection * n = (connection*)m_event_list[i].data.ptr;
			
			//read
			if (m_event_list[i].events & EPOLLIN){
				if(handle_recv(n) < 0){
					handle_close(n, recv_reason);
					continue;
				}
			}
			
			//write
			if(m_event_list[i].events & EPOLLOUT){
				if(handle_write(n) < 0){
					handle_close(n, write_reason);
					continue;
				}
			}

			//epoll error
			if (m_event_list[i].events & (EPOLLERR | EPOLLHUP)){
				handle_error(n);
			}
		}

		//timeout event
		evtime ev ;
		while(m_timer.pop_timeout(ev) != -1){
			handle_timer(&ev);
		}
	}

	warn_log("thread(%u) exit flag(%d)\n", pthread_self(), m_brun);
		
	return 0;
}


int worker::init(){

	//init timer module
	if(m_timer.init() < 0){
		return -1;
	}

	//create epoll events
	m_event_num = 1024 * 8;
	m_event_list = (epoll_event*)malloc(sizeof(epoll_event) * m_event_num); 
	if(!m_event_list){
		error_log("malloc event fail(%d)\n", errno);
		return -1;
	}
	
	//create epoll
	m_epfd = epoll_create(3200);
	if(m_epfd < 0){
		error_log("epoll_create error(%d)\n", errno);
		return -1;
	}
	
	//add timer
	m_timer.add(ev_sys_timer_active, itl_sys_timer_active, NULL);
	m_timer.add(ev_sys_timer_keepalive, itl_sys_timer_keepalive, NULL);
	
	//set run flag
	m_brun = true;
	
	//initialize
	on_initialize();
	
	return 0;
}

int worker::register_listen(int listenfd){
	//check wheather need to add listen fd

	struct epoll_event ev;
	m_listenfd = listenfd;
	ev.data.ptr = &m_listenfd ;
	ev.events =  EPOLLIN | EPOLLET | EPOLLPRI;
	if (epoll_ctl (m_epfd, EPOLL_CTL_ADD, m_listenfd, &ev) < 0) {
		error_log("epoll EPOLL_CTL_ADD error(%d)\n", errno);
		return -1;
	}
	return 0;
}

int worker::release(){
	//wait thread exit

	warn_log("worker exit\n");
	m_brun = false;

	//release timer
	m_timer.release();

	//close epoll
	if(m_epfd >=0){
		close(m_epfd);
		m_epfd = -1;
	}
	
	if(m_event_list){
		free(m_event_list);
		m_event_list = NULL;
	}
	m_event_num = 0;

	//close all connection
	list_node * pos  = m_list.begin();
	for(;pos != m_list.end(); pos = pos->next){
		connection * n = (connection*)pos->ptr;
		delete n;
	}

	return 0;
}

int worker::handle_accept(){

	time_t cur = get_system_time();
	for(;;){
		
		struct sockaddr_in peeraddr; 
		int len = sizeof(peeraddr); 

		int fd = accept(m_listenfd, (struct sockaddr *)&peeraddr, (socklen_t*)&len);
		if(fd < 0 && errno != EINTR){
			break;
		}

		if(make_no_block(fd) < 0){
			close(fd);
			error_log("make_no_block error(%d)\n", errno);
			continue;
		}

		int opt = 1024 * 128;
		if(setsockopt(fd, SOL_SOCKET ,SO_SNDBUF,(char *)&opt, sizeof(opt)) <0){
			warn_log("setsockopt SO_SNDBUF error(%d)\n", errno);
		}
		
		if(setsockopt(fd, SOL_SOCKET ,SO_RCVBUF,(char *)&opt, sizeof(opt)) < 0){
			warn_log("setsockopt SO_RCVBUF error(%d)\n", errno);
		}

		connection * n = new connection(m_epfd, fd);
		n->set_status(kconnected);
		n->set_peeraddr(peeraddr);
		n->set_alive_time(cur);
		n->set_list_node(m_list.push_back(n));
		on_accept(n);

		ipaddr &paddr = n->get_peeraddr();
		debug_log("accept from %s:%d socket(%d)\n", paddr.ip, paddr.port, fd);
	}

	if(errno != EAGAIN && errno != ECONNABORTED  && errno != EINTR){
		error_log("accept error(%d)\n", errno);
		return -1;
	}
	return 0;
}

int worker::handle_error(connection * n){
	error_log("socket error(%d)\n", errno);
	return handle_close(n, error_reason);
}

int worker::handle_close(connection * n, int reason){

	if(n->get_status() == kconnected){
		n->set_status(kdisconnected);
		m_list.remove(n->get_list_node());
		on_close(n, reason);
	}
	else if(n->get_status() == kconnecting){
		n->set_status(kdisconnected);
		on_connect(n);
	}
	else{
		warn_log("already closed\n");
	}
	delete n;
	return 0;
}


int worker::loop_unpack(connection * n){

	buffer * recvbuf = n->get_recv_buffer();
	int offset = 0;

	while(recvbuf->has > 0){

		char* packet = NULL;
		int pktlen = 0;

		int consume = on_unpack(recvbuf->buf + offset, recvbuf->has, pktlen, packet);

		if (consume < 0 || consume > recvbuf->has){
			error_log("unpack error consume(%d) has(%d)\n", consume, recvbuf->has);
			return -1;
		}

		if (!consume){
			if(recvbuf->has && offset){
				memcpy(recvbuf->buf, recvbuf->buf + offset, recvbuf->has);
			}
			return 0;
		}

		if (pktlen < 0  || pktlen > consume){
			error_log("unpack error pktlen(%d) consume(%d)\n", pktlen, consume);
			return -1;
		}

		recvbuf->has -= consume;
		offset += consume;

		if(packet && pktlen){
			on_recv(n, packet, pktlen);
		}
	}

	return 0;
}

int worker::handle_recv(connection * n){

	buffer * buf = n->get_recv_buffer();
	n->set_alive_time(get_system_time());

	for(;;){

		int left = buf->len - buf->has;	
		if(left <= 0){
			error_log("packet too large\n");
			return -1;
		}
		
		int recv_bytes  = recv(n->fd(), buf->buf + buf->has, left, 0);
		if(recv_bytes < 0){	
			if(errno != EINTR && errno != EAGAIN){
				error_log("recv(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
				return -1;
			}
			else{
				return 0;
			}
		}
		else if(recv_bytes == 0){
			warn_log("remote close(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
			return -1;
		}
		else{
			buf->has += recv_bytes;
		}

		if(loop_unpack(n) < 0){
			return -1;
		}

		m_list.move_tail(n->get_list_node());
		if(recv_bytes < left){
			return 0;
		}
	}
	return 0;
}


int worker::handle_write(connection * n){

	if(n->get_status() == kconnecting){
		return handle_connect(n);
	}
	else if(n->get_status() == kconnected){
		return n->post_send();
	}
	else{
		warn_log("socket is disconnected\n");
		return 0;
	}

}

int worker::handle_timer(evtime * e){

	switch(e->id){

	case ev_sys_timer_active:
		return timer_active(e);

	case ev_sys_timer_connect:
		return timer_connect(e);

	case ev_sys_timer_keepalive:
		return timer_keepalive(e);

	default:
		return on_timer(e->id, e->interval, e->ptr);
	}
}

int worker::timer_active(evtime*e){
	debug_log("server version 1.0 compile date: %s %s\n", __DATE__,__TIME__);
	return m_timer.add(e->id, e->interval, e->ptr);
}

int worker::timer_connect(evtime * e){
	
	connection * n = (connection*)e->ptr;
	ipaddr &peeraddr = n->get_peeraddr();
	struct sockaddr_in seraddr = {0};
	seraddr.sin_family = AF_INET; 
	seraddr.sin_addr.s_addr = inet_addr(peeraddr.ip); 
	seraddr.sin_port = htons(peeraddr.port); 
	int ret = connect(n->fd(), (sockaddr *)&seraddr, sizeof(sockaddr));
	if (ret < 0){
		if (errno != EINTR && errno != EINPROGRESS && errno != EISCONN){
			error_log("connect error(%d)\n", errno);
			on_connect(n);
			delete n;
		}
		else{
			n->set_status(kconnecting);
		}
	}
	else{
		n->set_alive_time(get_system_time());
		n->set_status(kconnected);
		n->set_list_node(m_list.push_back(n));
		on_connect(n);
	}
	
	return 0;
}

int worker::timer_keepalive(evtime*e){

	time_t cur = get_system_time();
	for(list_node * pos  = m_list.begin(); pos != m_list.end();){

		connection * n = (connection*)pos->ptr;
		int dis = (int)difftime(cur, n->get_alive_time());
		if(dis < m_keepalive){
			break;
		}
		warn_log("connection(%d) timed out(%d)\n", n->fd(), dis);
		pos = pos->next;
		handle_close(n, timeout_reason);
	}
	return m_timer.add(e->id, e->interval, e->ptr);
}

int worker::handle_connect(connection * n){

	int error = 0;
	int len = sizeof(error);
	int ret = getsockopt(n->fd(), SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
	
	if(ret < 0 || error){
		error_log("connect fail socket error(%d:%s)\n",errno, strerror(errno));
		return -1;
	}
	
	n->set_alive_time(get_system_time());
	n->set_status(kconnected);
	n->set_list_node(m_list.push_back(n));
	return on_connect(n);
}

int worker::post_connect(const char * ip, ushort port, int delay, void* context /*= NULL*/){

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1){	
		error_log("socket error(%d)\n", errno);
		return -1;
	}
	
	if(make_no_block(fd) < 0){
		close(fd);
		error_log("make_no_block error(%d)\n", errno);
		return -1;
	}
	
	ipaddr peeraddr = {{0}, port};
	strncpy(peeraddr.ip, ip, sizeof(peeraddr.ip) - 1);
	connection * n = new connection(m_epfd, fd);
	n->set_peeraddr(peeraddr);
	n->set_context(context);

	int ret = m_timer.add(ev_sys_timer_connect, delay, n);
	if(ret < 0){
		delete n;
	}
	return ret;
}

int worker::set_timer(int id, int interval, void * context /* = NULL */){
	return m_timer.add(id, interval, context);
}
 
int worker::set_abs_timer(int id, int year, int mon, int day, 
							   int hour, int min, int sec, void * context /* = NULL */){

	struct tm tnow = {0};
    tnow.tm_year = year - 1900;
    tnow.tm_mon  = mon - 1;
    tnow.tm_mday = day;
    tnow.tm_hour = hour;
    tnow.tm_min  = min;
    tnow.tm_sec  = sec;

	time_t tsecs = mktime(&tnow);
	if(tsecs == (time_t)(-1)){
		return -1;
	}

	int interval = (int)difftime(tsecs, get_system_time()) * 1000;
	return m_timer.add(id, interval, context);
}


int worker::disconnect(connection *n){
	if(n->get_status() != kconnected){
		error_log("already disconnected\n");
		return -1;
	}
	n->set_status(kdisconnected);
	m_list.remove(n->get_list_node());
	delete n;
	return 0;
}

void worker::set_keepalive(int timeout){
	m_keepalive = timeout < 0 ? 0x7fffffff : timeout;
}

inline time_t worker::get_system_time(){
	return time(NULL);
}

