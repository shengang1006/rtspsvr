#pragma once

#include "utility.h"


#define recv_buf_len    64 * 1024
#define send_buf_len    128 * 1024
struct packet_buf
{
	char *buf;    //包缓冲区
	int has;      //缓冲区数据位置
	int len;      //缓冲区大小
};

enum{knew,kadd,kdel};
enum{kconnected, kconnecting, kdisconnected};
enum{recv_reason, write_reason, timeout_reason, error_reason};
	
class connection
{
public:
	connection(int epfd, int fd);
	
	virtual ~connection();
	
	int fd();

	int post_send(char * data, int len);
		
	ipaddr & get_peeraddr();
	
	ipaddr & get_localaddr();
	
	int set_context(void * context);
	
	void* get_context();
	
	int set_tcp_no_delay(bool val);
	
	int add_ref();
	
	int release_ref();
	
	bool expired();
	
	int get_appid();
		
	int set_appid(int appid);
		
	bool   connected();
protected:

	time_t get_alive_time();
	
	int set_peeraddr(ipaddr & addr);
	int set_peeraddr(struct sockaddr_in & addr);
	
	int set_alive_time(time_t tick);

	int get_status();
	
	int set_status(int status);
		
	int set_inner_context(void * context);
	
	void * get_inner_context();
	
	packet_buf * get_recv_buf();
	
	int post_send();
	
	friend class server;
	friend class con_list;
	
protected:
	int init();
	int release();
	
	int enable_writing();
	int disable_writing();
	int update();
	
protected:
	int m_epfd; 
	int m_fd;
	int m_appid;
	uint m_events;
	packet_buf m_send_buf;
	packet_buf m_recv_buf;

	auto_mutex m_mutex;
	
	void* m_context;
	void* m_inner_context;
	ipaddr m_peeraddr;
	ipaddr m_localaddr;
	
	int m_operation;
	int m_status;
	int m_ref;
	
	
	time_t m_alive_time;
};