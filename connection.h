#pragma once
#include "tlist.h"

#define recv_buf_len    64 * 1024
#define send_buf_len    128 * 1024

struct packet_buf{
	char *buf;    //包缓冲区
	int has;      //缓冲区数据位置
	int len;      //缓冲区大小
};

enum{knew,kadd,kdel};
enum{kconnected, kconnecting, kdisconnected};
enum{recv_reason, write_reason, timeout_reason, error_reason};
	
class connection{
	
public:
	connection(int epfd, int fd);
	
	virtual ~connection();
	
	int fd();

	int post_send(char * data, int len);
		
	ipaddr & get_peeraddr();
	ipaddr & get_localaddr();
	
	void set_context(void * context);
	void* get_context();
	
	int set_tcp_no_delay(bool val);
	
	int add_ref();
	int release_ref();
	bool expired();
	
	bool connected();
protected:

	void set_peeraddr(const ipaddr & addr);
	void set_peeraddr(const struct sockaddr_in & addr);
	
	int  get_appid();
	void set_appid(int appid);
	
	time_t get_alive_time();
	void set_alive_time(time_t tick);

	int get_status();
	int set_status(int status);
		
	list_node * get_list_node();
	void set_list_node(list_node * ln);
	
	packet_buf * get_recv_buf();	
	int post_send();
	
	friend class server;	
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
	int m_events;
	int m_operation;
	int m_status;
	int m_ref;
	time_t m_alive_time;
		
	packet_buf m_send_buf;
	packet_buf m_recv_buf;

	ipaddr m_peeraddr;
	ipaddr m_localaddr;
		
	void* m_context;
	list_node * m_list_node;

	auto_mutex m_mutex;	
};