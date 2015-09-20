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

//include '\0'
#define max_task_len 16
#define max_path_len 512


typedef unsigned int uint;
typedef unsigned short ushort;
typedef uint64_t uint64;
typedef int64_t  int64;

typedef void*(*start_rtn)(void*);

/*system timer*/
enum{ 
	ev_sys_timer_active = 0,
	ev_sys_timer_connect,
	ev_sys_timer_keepalive,
	ev_sys_timer_user,   
};
enum{
	itl_sys_timer_active = 3600 * 1000,
	itl_sys_timer_keepalive = 30 * 1000,
};

/*ipaddr define*/
struct ipaddr{
	char ip[INET_ADDRSTRLEN];//INET_ADDRSTRLEN = 16
	ushort port;
};

/*auto auto_mutex*/
class auto_mutex{
	
public:
	auto_mutex();	
	~auto_mutex();
	void lock();	
	void unlock();
private:
	pthread_mutex_t m_mutex;
};

/*auto lock*/
class auto_lock{

public:
	auto_lock(auto_mutex & mutex);	
	~auto_lock();
private:
	auto_mutex & m_mutex;
};

/*ring buffer with write lock*/
class ring_buffer{

public:
	ring_buffer();	
	virtual~ ring_buffer();
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

/*create thread , suc 0*/
int create_thread(pthread_t * tid, start_rtn fun, const char * name, void * param);

/*create multi level directory , suc 0*/
int create_directory(const char * path, int amode = 777);

/*set no block fd, suc 0*/
int make_no_block(int fd);

/*get 64 bits time in msecs*/
int64 get_tick_count();

/*system simple log*/
int sys_log(FILE * fd, const char *format,...);
#define error_log(format, ...) sys_log (stderr, "%s[%d]: error: "format"", __func__, __LINE__, ##__VA_ARGS__)
#define debug_log(format, ...) sys_log (stdout, "%s[%d]: debug: "format"", __func__, __LINE__, ##__VA_ARGS__)
#define warn_log(format, ...)  sys_log (stderr, "%s[%d]: warn: "format"", __func__, __LINE__,  ##__VA_ARGS__)
