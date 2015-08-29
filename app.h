#pragma once

#include "connection.h"

enum{tcp_type = 0, app_type, timer_type};
enum{log_info = 0, log_debug, log_warn, log_error, log_none};

struct hd_app{
	
	int type;     
	int event;      //消息
	char * content;
	int length;     //消息长度 
	
	union{
		struct hd_tcp{
			connection * n;
		}tcp;
		
		struct hd_timer{
			int interval;
			void * ptr;
		}timer;
		
		struct hd_app{
		}app;
	}u;
};

class app
{
public:

	virtual int on_accept(connection * n) = 0;
	virtual int on_recv(connection * n, char * data, int len) = 0;
	virtual int on_close(connection * n, int reason) = 0;
	virtual int on_connect(connection * n) = 0;
	virtual int on_app(int event, char* content, int length) = 0;
	virtual int on_timer(int event, int interval, void * ptr) = 0;
	virtual int on_unpack(char * data, int len, int & packetlen, char *&packet) = 0;
	
	app();
	
	virtual ~app();
	
	int create(int appid, int msg_cout, const char * app_name);
	int push(hd_app * msg);
	int increase_drop_msg();
	const char * name();
	int get_appid();	
	int add_timer(int id, int interval, void * context = NULL);
	int add_abs_timer(int id, int year, int mon, int day, int hour, 
		int min, int sec, void * context = NULL);	
	int post_connect(const char * ip, ushort port, int delay, void * context = NULL);
	int post_app_msg(int dst, int event, void * content = NULL, int length = 0);
	
	static int log_out(int lev, const char * format,...);	
private:

	static void * app_run(void* param);
	
	int run();
		
private:	
	ring_buffer m_ring_buf;
	char m_name[max_app_name + 1];
	bool m_brun;
	int m_drop_msg;
	int m_appid;
};