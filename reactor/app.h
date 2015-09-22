#pragma once
#include "connection.h"
#include "timer.h"

enum{tcp_type = 0, app_type, timer_type};

enum{
 	ev_sys_connect_ok,       //连接成功
	ev_sys_connect_fail,     //连接失败
	ev_sys_accept,           //新的连接
	ev_sys_recv,             //接收消息
	ev_sys_close,            //关闭消息
};


/***********app_connection***************/
class app_connection: public connection{
public:
	app_connection(int epfd, int fd);
	int  get_appid();
	void set_appid(int appid);	
	int post_send(char * data, int len);
	int post_send();
protected:
	int m_appid;	
	auto_mutex m_mutex;	
};

/***********hd_app***************/
struct app_hd{
	
	int type;     
	int event;      
	int length;     //消息长度 
	char* content;  //消息
		
	union{
		struct tcp_data{
			app_connection * n;
		}tcp;
		
		struct timer_date{
			int interval;
			void * ptr;
		}timer;
		
		struct app_data{
		}app;
	}u;
};


/***********app_context***************/
struct app_context{
	void * ptr;
	int appid;
};

/***********app_timer***************/
class app_timer{
public:
	int init(int precision = 1000);
	int release();
	int add(int id, int interval, void* data);
	int pop_timeout(evtime & ev);
	int latency_time();
public:
	timer m_timer;
	int m_precision;
	auto_mutex m_mutex;	
};

/***********app***************/
class app{
	
public:
	virtual int on_accept(app_connection * n) = 0;
	virtual int on_recv(app_connection * n, char * data, int len) = 0;
	virtual int on_close(app_connection * n, int reason) = 0;
	virtual int on_connect(app_connection * n) = 0;
	virtual int on_app(int event, char* content, int length) = 0;
	virtual int on_timer(int event, int interval, void * ptr) = 0;
	virtual int on_unpack(char * data, int len, int & packetlen, char *&packet) = 0;
	
	app();
	
	virtual ~app();
	
	int create(int appid, int msg_cout, const char * app_name);
	int push(const app_hd & msg);
	const char * name();
	int get_appid();	
	int add_timer(int id, int interval, void * context = NULL);
	int add_abs_timer(int id, int year, int mon, int day, int hour, 
		int min, int sec, void * context = NULL);	
	int post_connect(const char * ip, ushort port, int delay, void * context = NULL);
	int post_app_msg(int dst, int event, void * content = NULL, int length = 0);
	
private:
	static void * app_run(void* param);
	int run();		
private:	
	auto_mutex m_push_mutex;
	ring_buffer m_ring_buf;
	char m_name[max_task_len + 1];
	bool m_brun;
	int m_drop_msg;
	int m_appid;
};
