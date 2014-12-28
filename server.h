#pragma once
#include "app.h"
#include "timer.h"
#include "conlist.h"
#include "log.h"

#define max_app_num     16

class protocol_parser
{
public:
	//失败-1，成功返回包长
	virtual int get_packet(char * data, int len, char *&packet) = 0;
};

/*
*/

class server
{
public:
	server(protocol_parser * p);
	
	virtual ~server();
	
	int init(short port, int reuse = 1);
	
    int loop();
	
	int init_log(const char* path, const char * name, int max_size = 8<<20);
	
	int init_telnet(const char * prompt, short port); 
	
	int reg_cmd(const char* name, void* func);
	
	int log_out(int lev, const char * format,...);
	
	int set_loglev(int lev);
	
	int get_loglev();
	
	int set_keepalive(int timeout);
	
	int register_app(app * a, int msg_count, const char * name, int app_mode = app_shared);
	
	int add_timer(int id, int interval, int appid, void * context = NULL);
	
	int post_connect(const char * ip, short port, int delay, int appid = -1, void * context = NULL);
	
	int post_app_msg(int dst, int event, void * content = NULL, int length = 0, int src = -1);
		
	int stop();
	
protected:
	
	int run();

	int handle_recv(connection * n);
	
	int handle_close(connection * n);
	
	int handle_accept();
	
	int handle_write(connection * n);
	
	int handle_timer(evtime * e);
	
	int packet_dispatch(connection * n);
	
	int post_msg(int dst, void* ptr, int from, int event, void * content, int length, int src);
	
	int post_con_msg(connection * n, int event, void * content = NULL, int length = 0);

	int handle_connect(connection * n);
	
	int check_keepalive(evtime * e);
	
	int check_invalid_con(evtime * e);
	
	int start_connect(evtime * e);

	int get_shared_app();
private:
	int m_epfd;
	int m_listenfd;
	protocol_parser * m_parser;
	app * m_apps[max_app_num];
	int m_app_num;
	int m_last_app;
	timer m_timer;
	int m_keepalive_timeout;
	con_list m_con_list;
	tellog m_tellog;
	int m_log_lev;
};


