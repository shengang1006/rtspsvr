#include <sys/epoll.h>
#include <netinet/tcp.h>
#include "connection.h"

#define EPOLLNONE 0x0

int init_buffer(buffer & s,int size){
	if(!s.buf){
		s.buf = (char*)malloc(size);
		s.has = 0;
		s.len = size;
	}
	if(!s.buf){
		return -1;
	}
	return 0;
}

void release_buffer(buffer & s){
	if(s.buf){
		free(s.buf);
		s.buf = 0;
	}
	s.has = 0;
	s.len = 0;
}

void inet_ntoa_convert(ipaddr & dst, const struct sockaddr_in & addr){
	unsigned char *p = (unsigned char*)&addr.sin_addr;
	sprintf(dst.ip,"%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	dst.port = ntohs(addr.sin_port);
}


connection::connection(int epfd, int fd){
	m_fd = fd;
	m_epfd = epfd;
	m_operation = knew;
	m_context = NULL;
	m_epoll_events = EPOLLNONE;
	m_status = kdisconnected;
	m_ref = 0;
	m_alive_time  = 0;
	m_list_node = NULL;
	memset(&m_send_buffer, 0, sizeof(m_send_buffer));
	memset(&m_recv_buffer, 0, sizeof(m_recv_buffer));
	memset(&m_localaddr,0, sizeof(m_localaddr));
	memset(&m_peeraddr,0, sizeof(m_peeraddr));
}

connection::~connection(){
	release();
}

int connection::fd(){
	return m_fd;
}

ipaddr& connection::get_peeraddr(){
	return m_peeraddr;
}
	
ipaddr& connection::get_localaddr(){
	return m_localaddr;
}

void connection::set_peeraddr(const ipaddr & addr){
	m_peeraddr = addr;
}

void connection::set_peeraddr(const struct sockaddr_in & addr){
	inet_ntoa_convert(m_peeraddr, addr);
}

list_node * connection::get_list_node(){
	return m_list_node;
}

void connection::set_list_node(list_node * ln){
	m_list_node = ln;
}

	
buffer* connection::get_recv_buffer(){
	return &m_recv_buffer;
}
	
bool connection::connected(){
	return m_status == kconnected;
}

void connection::set_context(void * context){
	m_context = context;
}
	
void* connection::get_context(){
	return m_context;
}


int connection::set_status(int status){

	m_status = status;
	if(status == kconnected){
		return init();
	}
	if(status == kdisconnected){
		return release();
	}
	if(status == kconnecting){
		return update_writing(true);
	}
	return 0;
}
	
int connection::get_status(){
	return m_status;
}
	
time_t connection::get_alive_time(){
	return m_alive_time;
}

void connection::set_alive_time(time_t tick){
	m_alive_time = tick;
}
	
int connection::init(){

	//add epoll event
	m_epoll_events = EPOLLIN | EPOLLET | EPOLLPRI;
	if(update_event() < 0){
		return -1;
	}

	//init recv buf
	if(init_buffer(m_recv_buffer, recvbuf_len) < 0){
		error_log("init init_fdbuf error(%d)\n", errno);
		return -1;
	}

	//get ip address
	struct sockaddr_in localaddr;
	int len = sizeof(localaddr);
	if (getsockname(m_fd, (struct sockaddr *)&localaddr, (socklen_t*)&len) == 0){
		inet_ntoa_convert(m_localaddr, localaddr);
	}
	else{
		error_log("getsockname error(%d)\n", errno);
	}
	
	return 0;
}

int connection::release(){	

	if(m_epoll_events != EPOLLNONE){
		m_epoll_events = EPOLLNONE;
		update_event();
	}	
	if(m_fd >= 0){
		close(m_fd);
		m_fd = -1;
	}
	release_buffer(m_send_buffer);
	release_buffer(m_recv_buffer);
	return 0;
}

int connection::update_writing(bool enable){

	int evs = m_epoll_events & EPOLLOUT;
	if(!evs && enable){
		m_epoll_events |= EPOLLOUT;
	}
	else if(evs && !enable){
		m_epoll_events &= ~EPOLLOUT;
	}
	else{
		warn_log("already update_writing enable(%d)\n", enable);
		return 0;
	}
	return update_event();
}

int connection::update_event(){

	struct epoll_event ev = {0};
	ev.data.ptr = (void*)this;
	ev.events = (m_epoll_events | EPOLLERR | EPOLLHUP);
	int ret = 0;

	if(m_operation == knew){
		m_operation = kadd;
		ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_fd, &ev);
	}
	else{
		if(m_epoll_events == EPOLLNONE){
			m_operation = knew;
			ret = epoll_ctl(m_epfd, EPOLL_CTL_DEL, m_fd, &ev);
		}
		else{
			ret = epoll_ctl(m_epfd, EPOLL_CTL_MOD, m_fd, &ev);
		}
	}

	if(ret < 0){
		error_log("update events error %d socket(%d)\n", errno, m_fd);	
	}
	
	return ret;
}

int connection::add_ref(){
	return __sync_fetch_and_add(&m_ref, 1);
}
	
int connection::release_ref(){

	int count = __sync_fetch_and_sub(&m_ref, 1);
	if(count == 0){
		error_log("memory leak\n");
	}
	return count;
}
	
bool connection::expired(){
	return __sync_bool_compare_and_swap(&m_ref, 0, 0);
}

int connection::set_tcp_no_delay(bool val){

	int opt = val ? 1: 0;
	int res = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY ,(char*)&opt, sizeof(opt));
	if(res < 0){
		error_log("set set_tcp_no_delay error(%d)\n", errno);
	}
	return res;
}

int connection::post_send(){

	if(EPOLLNONE == m_epoll_events){
		error_log("post send already closed socket(%d)\n", m_fd);
		return -1;
	}
	
	int len = m_send_buffer.has;
	if(len == 0){
		warn_log("post send no data socket(%d)\n", m_fd);
		return 0;
	}
	
	int total = 0;
	do{
		int sent = ::send(m_fd, m_send_buffer.buf + total , len - total, 0);
		if(sent  < 0 ){
			if(errno != EINTR && errno != EAGAIN){
				error_log("post send error(%d) socket(%d)\n", errno, m_fd);
				return -1;
			}
			else{
				break;
			}
		}
		total += sent;
		
	}while(total < len);
	
	m_send_buffer.has = len - total;
	if(!m_send_buffer.has){
		update_writing(false);
	}
	else{
		memcpy(m_send_buffer.buf, m_send_buffer.buf + total, m_send_buffer.has);
	}	
	return 0;
}

//
int connection::post_send(char * data, int len){

	if(!data || len <= 0){
		warn_log("post_send invalid data socket(%d)\n", m_fd);
		return 0;
	}
	
	if(EPOLLNONE == m_epoll_events){
		debug_log("post_send already closed socket(%d)\n", m_fd);
		return -1;
	}
	
	int pos = m_send_buffer.has;
	if(pos){
		debug_log("has left %d bytes, socket(%d)\n", pos + 1, m_fd);
		if(pos + len > m_send_buffer.len){
			warn_log("buffer overflow socket(%d)\n", m_fd);
			return -1;
		}
		else{
			memcpy(m_send_buffer.buf + pos, data, len);
			m_send_buffer.has += len;
			return 0;
		}
	}
	
	int total = 0;
	do{
		int sent = ::send(m_fd, data + total, len - total, 0);
		if(sent < 0){
			if(errno != EINTR && errno != EAGAIN){
				error_log("post_send error(%d) socket(%d)\n", errno, m_fd);
				return -1;
			}
			else{
				//push into send buf
				if(init_buffer(m_send_buffer, sendbuf_len) <0){
					error_log("post_send init_fdbuf error(%d)\n", errno);
					return -1;
				}
				memcpy(m_send_buffer.buf + m_send_buffer.has, data + total, len - total);
				m_send_buffer.has += len - total;
				warn_log("send full %d/%d error(%d) socket(%d)\n", total, len, errno, m_fd);	
				update_writing(true);
				return 0;
			}		
		}
		total += sent;
		
	}while(total < len);
		
	return 0;
}
