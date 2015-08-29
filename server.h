#pragma once
#include "app.h"
#include "timer.h"
#include "conlist.h"
#include "log.h"

#define max_app_num     16
#define max_udp_len     (2<<16) //64k

/*
*/

class server
{
private:
	server();
	server(const server &);  
    server & operator = (const server &); 
	
public:
	static server * instance();
	
	virtual ~server();
	
	int init();
	
	int create_tcp_server(ushort port, int reuse = 1);
		
    int loop();
	
	int init_log(const char* path, const char * name, int max_size = 8<<20);
	
	int set_keepalive(int timeout);
	
	int register_app(app * a, int msg_count, const char * name);
	
	int add_timer(int id, int interval, int appid, void * context = NULL);
	
	int add_abs_timer(int id, int year, int mon, int day, 
					  int hour, int min, int sec, int appid, void * context = NULL);

		
	int post_connect(const char * ip, ushort port, int delay, int appid, void * context = NULL);
	
	int post_app_msg(int dst, int event, void *content = NULL, int length = 0);
		
	int stop();
	
	int get_tcp_port();
	
protected:
	
	int run();

	int handle_recv(connection * n);
	
	int handle_close(connection * n, int reason);
	
	int handle_accept();
	
	int handle_write(connection * n);
	
	int handle_timer(evtime * e);
	
	int packet_dispatch(connection * n);
	
	int post_msg(int dst, hd_app * msg);
	
	int post_tcp_msg(connection * n, int event, void * content = NULL, int length = 0);
	
	int post_timer_msg(evtime * e);

	int handle_connect(connection * n);
	
	int check_keepalive(evtime * e);
	
	int check_invalid_con(evtime * e);
	
	int start_connect(evtime * e);

	int get_appid();
private:
	int m_epfd;
	int m_listenfd;
	int m_listen_port;
	
	app * m_apps[max_app_num];
	int m_app_num;
	int m_last_app;
	timer m_timer;
	int m_keepalive_timeout;
	con_list m_con_list;
};


