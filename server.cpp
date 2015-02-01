#include <sys/epoll.h>
#include <sys/types.h>  
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <sys/resource.h>
#include <signal.h> 
#include "server.h"

server::server(protocol_parser * p)
{
	m_epfd = -1;
	m_listenfd  = -1;
	m_parser = NULL;
	m_app_num = 0;
	m_last_app = -1;
	memset(m_apps, 0, sizeof(m_apps));
	m_parser = p;
	m_keepalive_timeout = 120;
	m_log_lev = log_debug;
}

server:: ~server()
{
	stop();
}
	
int server::run()
{
	int event_num = 1024 * 8;
	struct epoll_event * events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * event_num); 
	
	if(!events){
		return -1;
	}
	
	while(true){
	
		int timeout = -1;
		m_timer.timeout(timeout);
		 
		int res = epoll_wait (m_epfd, events, event_num, timeout);
		
		if (res == -1)
		{
			printf_t("error: epoll_wait error(%d %s)\n", errno, strerror(errno));
			if (errno != EINTR) 
			{
				return -1;
			}
			continue;
		}
		
		uint cur = (uint)time(NULL);
				
		for(int i = 0; i < res; i++)
		{
			//accept
			if (events[i].data.fd == m_listenfd && m_listenfd >= 0)
			{	
				handle_accept();
				continue;
			}
			
			connection * n = (connection*)events[i].data.ptr;
			
			//read
			if (events[i].events & EPOLLIN)
			{
				n->set_alive_time(cur);
				if(handle_recv(n) < 0)
				{
					handle_close(n);
					continue;
				}
			}
			
			//write
			if(events[i].events & EPOLLOUT)
			{
				if(handle_write(n) <0)
				{
					handle_close(n);
					continue;
				}
			}
			
			//error
			if (events[i].events & (EPOLLERR | EPOLLHUP ))
			{
				handle_close(n);
			}
		}
	
		//timer event
		evtime ev ;
		while(m_timer.pop_timeout(ev) != -1)
		{	
			handle_timer(&ev);
		}
	}
	
	if(events)
	{
		free(events);
	}
	
	return 0;
}

/*
void* ptr;     //消息节点，可以为空
	int appid;
	int from;  
	int event;      //消息
	int length;     //消息长度 
	char* content;  //消息内容
*/
int server::post_msg(int dst, void* ptr, int from, int event, void * content, int length, int src)
{
	if(dst < 0 || dst >= m_app_num)
	{
		printf_t("error: dst_app error\n");
		return -1;
	}
	
	app * a = m_apps[dst];
	if(!a)
	{
		printf_t("error: no app bind\n");
		return -1;
	}
	
	//md by 2015-1-20
	app_msg* msg = (app_msg*)malloc(sizeof(app_msg) + length + 1);
	if(!msg)
	{
		printf_t("error: malloc %d bytes fail, error(%d)\n", sizeof(app_msg) + length + 1, errno);
		return -1;
	}

	msg->ptr = ptr;
	msg->from = from;
	msg->src = src;
	msg->event = event;
	msg->length = length;
	
	if(length)
	{
		msg->content = (char*)msg + sizeof(app_msg) ;
		memcpy(msg->content, content, length);
		msg->content[length] = 0;	//add by 2015-1-20
	}
	else
	{
		msg->content = NULL;
	}
	
	if(a->push(msg) < 0)
	{
		int total = a->increase_drop_msg();
		free(msg);	
		printf_t("error: drop msg count(%d)\n", total);
		return -1;
	}
	return 0;
}

int server::post_app_msg(int dst, int event, void * content, int length, int src)
{
	return post_msg(dst, NULL, from_app, event, content, length, src);
}

int server::post_con_msg(connection * n, int event, void * content, int length)
{
	return post_msg(n->get_appid(), n, from_net, event, content, length, -1);
}

int server::get_shared_app()
{
	int count = 0;
	while(count++ < m_app_num)
	{
		m_last_app = (m_last_app + 1) % m_app_num;
		if(m_apps[m_last_app]->get_app_mode() == app_shared)
		{
			return m_last_app;
		}
	}
	return -1;
}

int server::packet_dispatch(connection * n)
{
	packet_buf * p_buf = n->get_recv_buf();
	char* packet = NULL;
	int packet_len = 0;	
	int offset = 0;
	
	while((packet_len = m_parser->get_packet(p_buf->buf + offset, p_buf->pos , packet)) >0)
	{
		p_buf->pos -= packet_len;
		offset += packet_len;

		//packet error
		if(p_buf->pos <0)
		{
			printf_t("error: packet error too len\n");
			break;
		}
		
		post_con_msg(n, ev_recv, packet, packet_len);
					
		if(p_buf->pos ==0)
		{
			break;
		}
	}
	
	//copy left
	if(p_buf->pos > 0 && offset > 0)
	{
		memcpy(p_buf->buf, p_buf->buf + offset, p_buf->pos);
	}
	
	//packet error
	if(packet_len < 0)
	{
		//close socket
		printf_t("error: packet error\n");
		return -1;
	}
	
	return 0;
}


int server::check_keepalive(evtime * e)
{
	uint cur = (uint)time(NULL);
	connection * n = m_con_list.go_first();
	while(n)
	{
		if(n->get_status() != kconnected || n->active_connect())
		{	
			n = m_con_list.go_next();
			continue;
		}
		
		int dis = (int)(cur - m_keepalive_timeout  - n->get_alive_time());
		if(dis <= 0)
		{
			//printf_t("debug: check alive_time(%u)\n", n->get_alive_time());
			break;
		}
		printf_t("warn : connection(%d) keep timeout(%d)\n", n->fd(), dis + m_keepalive_timeout);
		
		//fixed 2014-12-26
		connection * next =  m_con_list.go_next();
		handle_close(n);
		n = next;
	}
	m_timer.add(e->id, e->interval, e->ptr);
	return 0;
}


int server::check_invalid_con(evtime * e)
{
	connection * n = m_con_list.go_first();
	while(n)
	{	
		if(n->get_status() != kdisconnected)
		{
			break;
		}
		
		if(n->get_status() == kdisconnected && n->expired())
		{
			connection * next = m_con_list.go_next();
			m_con_list.remove(n);
			delete n;
			n = next;
			//printf_t("debug: invalid node delete\n");
		}
		else{
			n = m_con_list.go_next();
		}
	}
	m_timer.add(e->id, e->interval, e->ptr);
	return 0;
}


int server::start_connect(evtime * e)
{
	connection * n = (connection*)e->ptr;
	ipaddr peeraddr = n->get_peeraddr();

	struct sockaddr_in seraddr = {0};
	seraddr.sin_family = AF_INET; 
	seraddr.sin_addr.s_addr = inet_addr(peeraddr.ip); 
	seraddr.sin_port = htons(peeraddr.port); 

	int ret = connect(n->fd(), (sockaddr *)&seraddr, sizeof(sockaddr));
	
	if( n->get_appid() < 0)
	{
		n->set_appid(get_shared_app());
	}
	
	if (ret < 0)
	{	
		if (errno != EINTR && errno != EINPROGRESS && errno != EISCONN)
		{
			printf_t("error: connect error(%d)\n", errno);
			n->set_status(kdisconnected);
			m_con_list.push_front(n);
			post_con_msg(n, ev_connect_fail);
			return -1;
		}	
		n->set_status(kconnecting);
		m_con_list.push_back(n);
	}
	else
	{
		uint cur = (uint)time(NULL);
		n->set_status(kconnected);
		n->set_alive_time(cur);
		m_con_list.push_back(n);			
		post_con_msg(n, ev_connect_ok);
	}

	return 0;
}
	

int server::handle_timer(evtime * e)
{
	if(e->id == ev_timer_active)
	{
		m_timer.add(e->id, e->interval, e->ptr);
	}
	else if(e->id == ev_con_keepalive)
	{	
		check_keepalive(e);
	}
	else if(e->id == ev_con_clear)
	{
		check_invalid_con(e);
	}
	else if(e->id == ev_con_connect)
	{
		start_connect(e);
	}
	else
	{	
		post_msg(e->appid, e->ptr,from_timer, e->id, NULL, 0, -1);
	}
	
	return 0;
}


int server::handle_recv(connection * n)
{
	packet_buf * p_buf = n->get_recv_buf();		
	//et mode
	do 
	{
		int len = p_buf->len - p_buf->pos;
	
		if(len <= 0)
		{
			printf_t("error: packet too large\n");
			break;
		}
		
		int recv_bytes  = recv(n->fd(), p_buf->buf + p_buf->pos, len, 0);
	
		if(recv_bytes < 0)
		{	
			if(errno != EINTR && errno != EAGAIN)
			{
				printf_t("error: recv error(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
				return -1;
			}
			return 0;
		}
		else if(recv_bytes == 0)
		{
			printf_t("warn : remote close error(%d %s) socket(%d)\n", errno, strerror(errno), n->fd());
			return -1;
		}
		else
		{
			p_buf->pos += recv_bytes;
		}
		
		if(packet_dispatch(n) < 0)
		{
			return -1;
		}
		
		m_con_list.move_to_back(n);
			
		if(len != recv_bytes)
		{
			break;
		}
		
	} while (true);	
	return 0;
}


int server::handle_connect(connection * n)
{
	int error = 0;
	int len = sizeof(error);
	int ret = getsockopt(n->fd(), SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
	
	if(ret < 0 || error)
	{
		printf_t("error: connect fail socket(%d) ret(%d) error(%d %d)\n", n->fd(), ret, error, errno);
		return -1;
	}
	
	uint cur = (uint)time(NULL);
	n->set_alive_time(cur);
	n->set_status(kconnected);		
	m_con_list.move_to_back(n);
	post_con_msg(n, ev_connect_ok);
	
	return 0;
}

int server::handle_write(connection * n)
{
	if(n->get_status() == kconnecting)
	{
		return handle_connect(n);
	}
	else
	{
		return n->post_send();
	}
}

int server::handle_close(connection * n)
{
	if(n->get_status() == kconnected)
	{
		n->set_status(kdisconnected);
		m_con_list.move_to_front(n);
		post_con_msg(n, ev_close);
	}
	else if(n->get_status() == kconnecting)
	{
		n->set_status(kdisconnected);
		m_con_list.move_to_front(n);
		post_con_msg(n, ev_connect_fail);
	}
	else
	{
		printf_t("warn : handle_close already closed socket(%d)\n", n->fd());
	}

	return 0;
}

int server::handle_accept()
{

	int fd = -1;
	struct sockaddr_in peeraddr; 
	int addrlen = sizeof(peeraddr); 
	int listenfd = m_listenfd;
	uint cur = (uint)time(NULL);
	while((fd = accept(listenfd, (struct sockaddr *)&peeraddr, (socklen_t*)&addrlen)) >= 0)
	{	
			
		if (make_no_block(fd) < 0){
			printf_t("error: make_no_block error(%d)\n", errno);
			close(fd);
			continue;
		}
		
		int opt = 1024 * 128;
		if(setsockopt(fd, SOL_SOCKET ,SO_SNDBUF,(char *)&opt, sizeof(opt)) <0)
		{
			printf_t("error: set send buffer error(%d)\n", errno);
		}
	
		if(setsockopt(fd, SOL_SOCKET ,SO_RCVBUF,(char *)&opt, sizeof(opt)) < 0)
		{
			printf_t("error: set recv buffer error(%d)\n", errno);
		}
		
		//allocate appid		
		connection * n = new connection(m_epfd, fd);
		n->set_appid(get_shared_app());
		n->set_status(kconnected);
		n->set_active_connect(false);
		n->set_alive_time(cur);
		m_con_list.push_back(n);
		post_con_msg(n, ev_accept);
	}
	
	if(fd < 0)
	{
		if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
		{
			printf_t("error: net_accept.accept error\n");
			return -1;
		}
	}
	return 0;
}

int server::set_keepalive(int timeout)
{
	m_keepalive_timeout = timeout < 0 ? 120 : timeout;
	return 0;
}

int server::init_log(const char* path, const char * name, int max_size)
{
	return m_tellog.init_log(path, name, max_size);
}
	
int server::init_telnet(const char * prompt, short port)
{
	return m_tellog.init_telnet(prompt, port);
}

int server::reg_cmd(const char* name, void* func)
{
	return m_tellog.reg_cmd(name, func);
}

int server::set_loglev(int lev)
{
	m_log_lev = lev;
	return 0;
}

int server::get_loglev()
{
	return m_log_lev;
}

int server::log_out(int lev, const char * format, ...)
{
	if(lev < m_log_lev)
	{
		return 0;
	}
	
	char ach_msg[max_log_len] = {0};
	
	time_t cur_t = time(NULL);
    struct tm cur_tm = *localtime(&cur_t);
	
	if(lev != log_none)
	{
		sprintf(ach_msg, 
			"[%d-%02d-%02d %02d:%02d:%02d]",
			 cur_tm.tm_year + 1900, 
			 cur_tm.tm_mon + 1,
			 cur_tm.tm_mday,
			 cur_tm.tm_hour,
			 cur_tm.tm_min, 
			 cur_tm.tm_sec);
    }
	
	char * pos = ach_msg + strlen(ach_msg);
	
	int color = color_green;
	switch(lev)
	{	
		case log_error: 
			color = color_red;
			strcpy(pos, "[error]");
		break;
		case log_warn : 
			color = color_yellow; 
			strcpy(pos, "[warn]");
		break;
		case log_debug: 
			color = color_green;
			strcpy(pos, "[debug]");
		break;
		case log_info:
			color = color_white;
			strcpy(pos, "[info]");
		break;
		default:
			color = color_purple;
		break;
	}
	pos += strlen(pos);
	
    int nsize = max_log_len - (int)(pos - ach_msg);
		 	
    va_list pv_list;
    va_start(pv_list, format);	
    vsnprintf(pos, nsize, format, pv_list); 
    va_end(pv_list);
	
	return m_tellog.print(ach_msg, color);
}


int server::init(short port, int reuse /*= 1*/)
{
	//resource limit set 2015-1-23
	struct rlimit limit;
	limit.rlim_cur = 60000;
	limit.rlim_max = 60000;
	if(setrlimit(RLIMIT_NOFILE, &limit) < 0)
	{
		printf_t("error: setrlimit error(%d)\n", errno);
	}
	
	//ignore socket pipe 2015-1-23
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if(sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1) 
	{ 
		printf_t("error: sigaction error(%d)\n", errno);
	}
	
	//ignore thread pipe 2015-1-23
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) < 0)
	{
		printf_t("error: pthread_sigmask error(%d)\n", errno);
	}

	//init timer
	if(m_timer.init(0) < 0)
	{
		printf_t("error: timer init fail\n");
		return -1;
	}
	
	//init log module
	if(m_tellog.init() < 0)
	{
		printf_t("error: tellog init fail\n");
		return -1;
	}
	
	//add system timer event
	m_timer.add(ev_timer_active,  500, NULL);
	m_timer.add(ev_con_keepalive, 30000, NULL);
	m_timer.add(ev_con_clear, 5000, NULL);

	//create epoll
	m_epfd = epoll_create (32000);
	if (m_epfd < 0) 
	{
		printf_t("error: epoll create error(%d)\n", errno);
		return -1;
	}
	
	//check weather if need create tcp listen
	if(port <= 0){
		printf_t("warn: port <= 0, not create listen socket\n");
		return 0;
	}
	
	//create tcp listen socket
	m_listenfd = create_tcp_listen(port, reuse, SOMAXCONN);
	if(m_listenfd < 0)
	{
		printf_t("error: create_tcp_listen error(%d)\n", errno);
		return -1;
	}
	
	//add listen fd
	struct epoll_event ev;
	ev.data.fd = m_listenfd;
	ev.events =  EPOLLIN | EPOLLET | EPOLLPRI;
	if (epoll_ctl (m_epfd, EPOLL_CTL_ADD, m_listenfd, &ev) < 0) {
		printf_t("error: epoll EPOLL_CTL_ADD error(%d)\n", errno);
		return -1;
	}
	return 0;
}


int server::loop()
{
	prctl(PR_SET_NAME, "epoll");
	if(!m_app_num)
	{
		return -1;
	}
	return run();
}

int server::add_timer(int id, int interval, int appid, void * context)
{
	if(appid < 0)
	{
		return -1;
	}
	return m_timer.add(id, interval, context, appid) ? 0 : -1;
}


int server::register_app(app * a, int msg_count, const char * name,  int app_mode)
{
	if(!a || m_app_num >= max_app_num){
		return -1;
	}

	if(a->create(m_app_num, msg_count, name, app_mode) < 0)
	{
		delete a;
		return -1;
	}
	
	m_apps[m_app_num] = a;

	return m_app_num++;
}

int server::post_connect(const char * ip, short port, int delay, int appid ,void * context)
{
	if(delay < 0 || port <= 0)
	{
		printf_t("error: delay(%d) < 0 or port(%d) <= 0\n", delay, port);
		return -1;
	}
	
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{	
		printf_t("error: post_connect create socket error(%d)\n", errno);
		return -1;
	}
	
	if(make_no_block(fd) < 0)
	{
		close(fd);
		printf_t("error: post_connect make_no_block error(%d)\n", errno);
		return -1;
	}
	
	ipaddr peeraddr;
	memset(&peeraddr, 0, sizeof(peeraddr));
	strncpy(peeraddr.ip, ip, sizeof(peeraddr.ip));
	peeraddr.port = port;
	
	connection * n = new connection(m_epfd, fd);
	n->set_active_connect(true);
	n->set_peeraddr(peeraddr);
	n->set_context(context);
	n->set_appid(appid);
		
	if(!m_timer.add(ev_con_connect, delay, n))
	{
		delete n;
		return -1;
	}
	return 0;
}


int server::stop()
{
	
	if(m_epfd >= 0){
	
		close(m_epfd);
		m_epfd = -1;
	}
	
	if(m_listenfd >= 0){
		close(m_listenfd);
		m_listenfd = -1;
	}
	
	if(m_parser)
	{
		delete m_parser;
		m_parser = NULL;
	}
	
	m_timer.release();
	
	for(int k = 0; k < m_app_num; k++)
	{
		delete m_apps[k];
	}
	m_app_num = 0;

	return 0;
}


