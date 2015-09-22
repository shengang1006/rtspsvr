#pragma once
#include "tlist.h"

#define recvbuf_len    64 * 1024
#define sendbuf_len    128 * 1024
enum{knew,kadd};
enum{kconnected, kconnecting, kdisconnected};
enum{recv_reason, write_reason, timeout_reason, error_reason};

struct buffer{
	char *buf;    //包缓冲区
	int has;      //缓冲区数据位置
	int len;      //缓冲区大小
};

class connection{

public:
	connection(int epfd, int fd);
	virtual ~connection();
	
	int fd();
			
	ipaddr& get_peeraddr();
	ipaddr& get_localaddr();	
	bool connected();
	void set_context(void * context);	
	void* get_context();
	int set_tcp_no_delay(bool val);	

	int add_ref();	
	int release_ref();	
	bool expired();

	int init();
	int release();

	int post_send(char * data, int len);
	int post_send();

	void set_peeraddr(const ipaddr & addr);
	void set_peeraddr(const struct sockaddr_in & addr);

	list_node * get_list_node();
	void set_list_node(list_node * ln);

	int get_status();
	int set_status(int status);	

	time_t get_alive_time();
	void set_alive_time(time_t tick);
	
	buffer* get_recv_buffer();	
	
protected:
	int update_writing(bool enable);
	int update_event();
	
protected:
	int m_epfd; 
	int m_fd;
	int m_epoll_events;
	int m_operation;
	int m_status;
	int m_ref;
	time_t m_alive_time;

	buffer m_send_buffer;
	buffer m_recv_buffer;

	ipaddr m_peeraddr;
	ipaddr m_localaddr;
	
	void* m_context;
	list_node * m_list_node;
};
