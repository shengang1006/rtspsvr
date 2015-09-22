#include <sys/epoll.h>
#include <sys/types.h>  
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <sys/resource.h>
#include <signal.h> 
#include "server.h"

server::server(){
	m_epfd = -1;
	m_listenfd  = -1;
	m_listen_port  = -1;
	m_app_num = 0;
	m_last_app = -1;
	memset(m_apps, 0, sizeof(m_apps));
	m_keepalive = 120;
}


server:: ~server(){
	stop();
}

server * server::instance(){
	static server inst;
	return &inst;
}

int server::run(){
	
	int event_num = 1024 * 8;
	struct epoll_event * events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * event_num); 
	
	if(!events){
		return -1;
	}
	
	while(true){
	
		int timeout = m_timer.latency_time();
		
		int res = epoll_wait (m_epfd, events, event_num, timeout);
		
		if (res == -1){
			error_log("epoll_wait error(%d %s)\n", errno, strerror(errno));
			if (errno != EINTR){
				break;
			}
			else{
				continue;
			}
		}
	
		time_t cur = time(NULL);	
		for(int i = 0; i < res; i++){
			//accept
			if (events[i].data.ptr == &m_listenfd){	
				handle_accept();
				continue;
			}
			
			app_connection * n = (app_connection*)events[i].data.ptr;
			//read
			if (events[i].events & EPOLLIN){
				n->set_alive_time(cur);
				if(handle_recv(n) < 0){
					handle_close(n, recv_reason);
					continue;
				}
			}
			
			//write
			if(events[i].events & EPOLLOUT){
				if(handle_write(n) <0){
					handle_close(n, write_reason);
					continue;
				}
			}
			
			//error
			if (events[i].events & (EPOLLERR | EPOLLHUP )){
				handle_close(n, error_reason);
			}
		}
	
		//timer event
		evtime ev ;
		while(m_timer.pop_timeout(ev) != -1){	
			handle_timer(&ev);
		}
	}
	
	if(events){
		free(events);
	}
	
	return 0;
}

int server::get_tcp_port(){
	return 	m_listen_port;
}
	
int server::post_app_msg(int dst, int event, void * content, int length){
	
	if(dst < 0 || dst >= m_app_num){
		error_log("appid(%d) error\n", dst);
		return -1;
	}
	app_hd  msg = {0};
	msg.event = event;
	msg.content = (char*)content;
	msg.length = length;
	msg.type = app_type;

	return m_apps[dst]->push(msg);
}

int server::post_tcp_msg(app_connection * n, int event, void * content, int length){
	
	app_hd  msg = {0};
	msg.event = event;
	msg.content = (char*)content;
	msg.length = length;
	msg.type = tcp_type;
	msg.u.tcp.n = n;
	int ret = m_apps[n->get_appid()]->push(msg);
	if(ret < 0){
		if(event == ev_sys_connect_fail){
			delete n;
		}
		error_log("post_tcp_msg fail, event(%d)\n", event);
	}
	return ret;
}

int server::post_timer_msg(evtime * e){
	
	app_context * ac = (app_context *)e->ptr;
	app_hd msg = {0};
	msg.event = e->id;
	msg.type = timer_type;
	msg.u.timer.ptr = ac->ptr;
	msg.u.timer.interval = e->interval;
	int ret =  m_apps[ac->appid]->push(msg);
	free(ac);
	if(ret < 0){
		error_log("post_timer_msg fail, event(%d)\n", e->id);
	}
	return ret;
}

int server::allot_appid(){
	return m_last_app = (m_last_app + 1) % m_app_num;
}

int server::loop_unpack(app_connection * n){
	
	buffer * buf = n->get_recv_buffer();
	int offset = 0;
	app * a = m_apps[n->get_appid()];
	
	while(buf->has > 0){
		
		char* packet = NULL;
		int pktlen = 0;
		int consume = a->on_unpack(buf->buf + offset, buf->has ,pktlen, packet);
		
		if (consume < 0 || consume > buf->has){
			error_log("unpack error consume(%d) has(%d)\n", consume, buf->has);
			return -1;
		}

		if (!consume){
			if(buf->has && offset){
				memcpy(buf->buf, buf->buf + offset, buf->has);
			}
			return 0;
		}

		if (pktlen < 0  || pktlen > consume){
			error_log("unpack error pktlen(%d) consume(%d)\n", pktlen, consume);
			return -1;
		}
		
		buf->has -= consume;
		offset += consume;
		
		//ignore empty packet
		if(pktlen && packet){
			post_tcp_msg(n, ev_sys_recv, packet, pktlen);	
		}	
	}
	
	return 0;
}


int server::timer_keepalive(evtime * e){
	
	time_t cur = time(NULL);
	
	for(list_node * pos = m_list.begin(); pos != m_list.end();){
		
		app_connection* n = (app_connection*)pos->ptr;
		int dis = (int)difftime(cur, n->get_alive_time());
		if(dis < m_keepalive){
			break;
		}
		warn_log("app_connection(%d) time out(%d)\n", n->fd(), dis);	
		pos = pos->next;
		handle_close(n, timeout_reason);
	}
	return m_timer.add(e->id, e->interval, e->ptr);
}

int server::timer_connect(evtime * e){
	
	app_connection * n = (app_connection*)e->ptr;
	ipaddr &peeraddr = n->get_peeraddr();
	struct sockaddr_in seraddr = {0};
	seraddr.sin_family = AF_INET; 
	seraddr.sin_addr.s_addr = inet_addr(peeraddr.ip); 
	seraddr.sin_port = htons(peeraddr.port); 

	int ret = connect(n->fd(), (sockaddr *)&seraddr, sizeof(sockaddr));
	if (ret < 0){	
		if (errno != EINTR && errno != EINPROGRESS && errno != EISCONN){
			error_log("connect error(%d)\n", errno);
			post_tcp_msg(n, ev_sys_connect_fail);
		}
		else{
			n->set_status(kconnecting);
		}
	}
	else{
		n->set_status(kconnected);
		n->set_alive_time(time(NULL));
		n->set_list_node(m_list.push_back(n));			
		post_tcp_msg(n, ev_sys_connect_ok);
	}
	return 0;
}

int server::timer_active(evtime*e){
	debug_log("libserver version 1.0 compile date: %s %s\n", __DATE__,__TIME__);
	return m_timer.add(e->id, e->interval, e->ptr);
}

int server::handle_timer(evtime * e){
	
	if(e->id == ev_sys_timer_keepalive){	
		timer_keepalive(e);
	}
	else if(e->id == ev_sys_timer_active){
		timer_active(e);
	}
	else if(e->id == ev_sys_timer_connect){
		timer_connect(e);
	}
	else{	
		post_timer_msg(e);
	}
	return 0;
}


int server::handle_recv(app_connection * n)
{
	buffer * buf = n->get_recv_buffer();		
	//et mode
	for(;;) {
		
		int left = buf->len - buf->has;
		if(left <= 0){
			error_log("packet too large\n");
			break;
		}
		
		int recv_bytes  = recv(n->fd(), buf->buf + buf->has, left, 0);	
		if(recv_bytes < 0){	
			if(errno != EINTR && errno != EAGAIN){
				error_log("recv error(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
				return -1;
			}
			return 0;
		}
		else if(recv_bytes == 0){
			error_log("remote close error(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
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


int server::handle_connect(app_connection * n){
	int error = 0;
	int len = sizeof(error);
	int ret = getsockopt(n->fd(), SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
	
	if(ret < 0 || error){
		error_log("connect fail socket(%d) ret(%d) error(%d %d)\n", n->fd(), ret, error, errno);
		return -1;
	}
	
	n->set_alive_time(time(NULL));
	n->set_status(kconnected);		
	n->set_list_node(m_list.push_back(n));	
	post_tcp_msg(n, ev_sys_connect_ok);		
	
	debug_log("connect ok socket(%d)\n", n->fd());
	
	return 0;
}

int server::handle_write(app_connection * n){
	if(n->get_status() == kconnecting){
		return handle_connect(n);
	}
	else{
		return n->post_send();
	}
}

int server::handle_close(app_connection * n,  int reason){
	
	if(n->get_status() == kconnected){
		n->set_status(kdisconnected);
		m_list.remove(n->get_list_node());
		post_tcp_msg(n, ev_sys_close, &reason, sizeof(reason));
	}
	else if(n->get_status() == kconnecting){
		n->set_status(kdisconnected);
		post_tcp_msg(n, ev_sys_connect_fail);
	}
	else{
		warn_log("handle_close already closed socket(%d)\n", n->fd());
	}

	return 0;
}

int server::handle_accept(){
	
	time_t cur = time(NULL);
	for(;;){	
	
		struct sockaddr_in peeraddr; 
		int len = sizeof(peeraddr);
		
		int fd = accept(m_listenfd, (struct sockaddr *)&peeraddr, (socklen_t*)&len);
		if(fd < 0 && errno != EINTR){
			break;
		}
		
		if (make_no_block(fd) < 0){
			error_log("make_no_block error(%d)\n", errno);
			close(fd);
			continue;
		}
		
		int opt = 1024 * 128;
		if(setsockopt(fd, SOL_SOCKET ,SO_SNDBUF,(char *)&opt, sizeof(opt)) <0){
			error_log("set send buffer error(%d)\n", errno);
		}
	
		if(setsockopt(fd, SOL_SOCKET ,SO_RCVBUF,(char *)&opt, sizeof(opt)) < 0){
			error_log("set recv buffer error(%d)\n", errno);
		}
		
		//allocate appid		
		app_connection * n = new app_connection(m_epfd, fd);
		n->set_appid(allot_appid());
		n->set_status(kconnected);
		n->set_peeraddr(peeraddr);
		n->set_alive_time(cur);
		n->set_list_node(m_list.push_back(n));
		post_tcp_msg(n, ev_sys_accept);
		
		ipaddr &paddr = n->get_peeraddr();
		debug_log("accept from %s:%d  socket(%d)\n",paddr.ip, paddr.port, fd);
	}
	
	
	if(errno != EAGAIN && errno != ECONNABORTED  && errno != EINTR){
		error_log("net_accept.accept error\n");
		return -1;
	}
	
	return 0;
}

void server::set_keepalive(int timeout){
	m_keepalive = timeout < 0 ? 120 : timeout;
}

int server::init(){
	//resource limit set 2015-1-23
	struct rlimit limit;
	limit.rlim_cur = 60000;
	limit.rlim_max = 60000;
	if(setrlimit(RLIMIT_NOFILE, &limit) < 0){
		error_log("setrlimit error(%d)\n", errno);
	}
	
	//ignore socket pipe 2015-1-23
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if(sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1){ 
		error_log("sigaction error(%d)\n", errno);
	}
	
	//ignore thread pipe 2015-1-23
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) < 0){
		error_log("pthread_sigmask error(%d)\n", errno);
	}

	//create epoll
	m_epfd = epoll_create (32000);
	if (m_epfd < 0) {	
		error_log("epoll create error(%d)\n", errno);
		return -1;
	}
	
		//init timer
	if(m_timer.init() < 0){
		error_log("timer init fail\n");
		close(m_epfd);
		return -1;
	}
	
	//add system timer event
	m_timer.add(ev_sys_timer_active,  3600 * 1000, NULL);
	m_timer.add(ev_sys_timer_keepalive, 15000, NULL);	
	return 0;
}


int server::create_tcp_server(ushort port, int reuse /*= 1*/){
	
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);	
	if(listenfd < 0){
		error_log("create listen socket error(%d)\n", errno);
		return -1;
	}
	
	if (make_no_block(listenfd) < 0){
		error_log("fcnt getfl error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
		error_log("setsockopt SO_REUSEADDR error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	// bind & listen    
	sockaddr_in sin ;         
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;
	
	if(bind(listenfd, (const sockaddr*)&sin, sizeof(sin)) < 0){
		error_log("socket bind error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	if(listen(listenfd, SOMAXCONN) < 0){
		error_log("listen error(%d)\n", errno);
		close(listenfd);
		return -1;
	}

	//add listen fd
	m_listenfd = listenfd;
	m_listen_port = port;
	
	struct epoll_event ev;
	ev.data.ptr = &m_listenfd;
	ev.events =  EPOLLIN | EPOLLET | EPOLLPRI;
	if (epoll_ctl (m_epfd, EPOLL_CTL_ADD, m_listenfd, &ev) < 0) {
		error_log("epoll EPOLL_CTL_ADD error(%d)\n", errno);
		close(m_listenfd);
		return -1;
	}

	return 0;
}

int server::loop(){
	
	prctl(PR_SET_NAME, "epoll");
	if(!m_app_num){
		return -1;
	}
	return run();
}

int server::add_timer(int id, int interval, int appid, void * context){
	app_context * ac = (app_context*)malloc(sizeof(app_context));
	if(!ac){
		return -1;
	}
	
	ac->ptr = context;
	ac->appid = appid;
	if(m_timer.add(id, interval, ac) < 0){
		free(ac);
		return -1;
	}
	return 0;
}

int server::add_abs_timer(int id, int year, int mon, int day, 
						  int hour, int min, int sec, int appid,  void * context /* = NULL */){

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

	time_t curtime;
	curtime = time(NULL);
	int interval = (int)difftime(tsecs, curtime) * 1000;
	
	return add_timer(id, interval, appid, context);
}

int server::register_app(app * a, int msg_count, const char * name){
	
	if(!a || m_app_num >= max_app_num){
		return -1;
	}

	if(a->create(m_app_num, msg_count, name) < 0){
		delete a;
		return -1;
	}
	
	m_apps[m_app_num] = a;

	return  m_app_num++;
}

int server::post_connect(const char * ip, ushort port, int delay, int appid , void * context){
	if(delay < 0){
		error_log("delay(%d) <= 0\n", delay);
		return -1;
	}
	
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1){	
		error_log("post_connect create socket error(%d)\n", errno);
		return -1;
	}
	
	if(make_no_block(fd) < 0){
		close(fd);
		error_log("post_connect make_no_block error(%d)\n", errno);
		return -1;
	}
	
	ipaddr peeraddr = {{0}, port};
	strncpy(peeraddr.ip, ip, sizeof(peeraddr.ip) - 1);
	
	app_connection * n = new app_connection(m_epfd, fd);
	n->set_peeraddr(peeraddr);
	n->set_context(context);
	n->set_appid(appid);
		
	if(m_timer.add(ev_sys_timer_connect, delay, n) < 0){
		delete n;
		return -1;
	}
	return 0;
}


int server::stop(){
	exit(0);
	return 0;
}

