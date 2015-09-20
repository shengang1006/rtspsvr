#pragma once

#include "worker.h"
#define  max_worker_number 8
class watcher
{
private:
	watcher();
	watcher(const watcher &);  
    watcher & operator = (const watcher &);
	
public:

	static watcher * instance();
	virtual~watcher();

	int init();
	int create_tcp_server(ushort port, int reuse = 1);
	int tcp_port();
	int reg_worker(worker * s);
	int loop();

protected:
	static void * worker_task(void*param);

private:
	int m_listenfd;
	int m_port;
	int m_worker_number;
	worker * m_workers[max_worker_number];
};

