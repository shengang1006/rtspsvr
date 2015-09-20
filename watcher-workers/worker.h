#pragma once
#include <sys/epoll.h>
#include "timer.h"
#include "connection.h"

class worker
{
public:
	worker();
	virtual ~worker();

protected:

	virtual int on_initialize() = 0;
	virtual int on_accept(connection * n) = 0;
	virtual int on_recv(connection * n, char * data, int len) = 0;
	virtual int on_close(connection * n, int reason) = 0;
	virtual int on_connect(connection * n) = 0;
	virtual int on_timer(int event, int interval, void * ptr) = 0;
	virtual int on_unpack(char * data, int len, int& packlen, char *&packet) = 0;

	int post_connect(const char * ip, ushort port, int delay, void*context = NULL);
	int set_timer(int id, int interval, void * context = NULL);
	int set_abs_timer(int id, int year, int mon, int day, int hour, 
		int min, int sec, void * context = NULL);
	int disconnect(connection *n);
	void set_keepalive(int timeout);
	time_t get_system_time();
	
	friend class watcher;
private:
	
	int init();
	int register_listen(int listenfd);
	int release();
	int run();
	int handle_accept();
	int handle_connect(connection * n);
	int handle_recv(connection * n);
	int handle_write(connection * n);	
	int handle_timer(evtime * e);
	int handle_error(connection * n);
	int handle_close(connection * n, int reason);
	int loop_unpack(connection * n);
	int timer_active(evtime * e);
	int timer_connect(evtime * e);
	int timer_keepalive(evtime * e);
private:
	int m_epfd;
	int m_event_num;
	epoll_event * m_event_list;
	int m_listenfd;
	int m_keepalive;
	timer m_timer;
	tlist m_list;
	bool  m_brun;
};