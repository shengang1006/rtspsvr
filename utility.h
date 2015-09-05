#pragma once

#include <pthread.h>
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> 
#include <netinet/in.h>  
#include <time.h>
#include <sys/time.h> 
#include <semaphore.h>
#include <stdarg.h>
#include <sys/prctl.h>

#ifndef PR_SET_NAME
#define PR_SET_NAME 15  
#endif
#ifndef PR_GET_NAME
#define PR_GET_NAME 16
#endif

#define max_app_name 16

typedef unsigned int uint;
typedef unsigned short ushort;
typedef uint64_t uint64;
typedef int64_t  int64;
typedef void* (*thread_fun)(void * param);
typedef intptr_t intptr;

enum{ 

	ev_timer_active = 0, //激活 定时器
	ev_timer_keepalive,    //心跳检测 定时器
	ev_timer_connect,      //连接 定时器
 
   	ev_connect_ok,       //连接成功
	ev_connect_fail,     //连接失败
	ev_accept,           //新的连接
	ev_recv,             //接收消息
	ev_close,            //关闭消息
	
	ev_system_end,   
};

struct ipaddr{
	char ip[INET_ADDRSTRLEN];//INET_ADDRSTRLEN = 16
	ushort port;
};

class auto_mutex{
public:
	auto_mutex();	
	~auto_mutex();
	void lock();
	void unlock();
private:
	pthread_mutex_t m_mutex;
};

class auto_lock{
public:
	auto_lock(auto_mutex & mutex);	
	~auto_lock();
private:
	auto_mutex & m_mutex;
};


class ring_buffer{
	
public:
	ring_buffer();	
	virtual~ring_buffer();	
	int create(int size);
	int push(void* data);	
	int pop(void *&data, int timeout = -1);
	
protected:
	int m_write;
	int m_read;
	void ** m_buf;
	int m_size;
	sem_t m_hsem;
};


int create_thread(pthread_t * tid, thread_fun fun, const char * name, void * param);

int create_directory(const char * path, int amode = 777);

int make_no_block(int fd);

int64 get_tick_count();

/*system simple log*/
int sys_log(FILE * fd, const char *format,...);
#define error_log(format, ...) sys_log (stderr, "%s[%d]: error: "format"", __func__, __LINE__, ##__VA_ARGS__)
#define debug_log(format, ...) sys_log (stdout, "%s[%d]: debug: "format"", __func__, __LINE__, ##__VA_ARGS__)
#define warn_log(format, ...)  sys_log (stderr, "%s[%d]: warn: "format"", __func__, __LINE__,  ##__VA_ARGS__)
