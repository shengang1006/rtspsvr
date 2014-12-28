#include "utility.h"
#include <unistd.h>
#include <fcntl.h>


/********************************************************/
auto_mutex::auto_mutex()
{
	pthread_mutex_init(&m_mutex, NULL);
}
auto_mutex::~auto_mutex()
{
	pthread_mutex_destroy(&m_mutex);
}

void auto_mutex::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void auto_mutex::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}


auto_lock::auto_lock(auto_mutex & mutex)
	:m_mutex(mutex)
{
	m_mutex.lock();
}
auto_lock::~auto_lock()
{
	m_mutex.unlock();
}


/********************************************************/
ring_buffer::ring_buffer()
{
	m_buf = NULL;
	m_size = 0;
	m_read = 0;
	m_write = 0;
}

ring_buffer::~ring_buffer()
{
	if(m_buf)
	{
		free(m_buf);
		m_buf = NULL;
	}
	
	if(sem_destroy(&m_hsem) == -1)
	{
		printf_t("error :sem_destroy fail %d\n", errno);
	}
	m_size = 0;
	m_read = 0;
	m_write = 0;
}

int ring_buffer::create(int size)
{	
	if(m_buf)
	{
		return -1;
	}
	if(sem_init(&m_hsem, 0, 0) == -1)
	{
		printf_t("error :sem_init fail %d\n", errno);
		return -1;
	}
	
	m_buf = (void**)malloc(sizeof(void*)*size);
	if(!m_buf){
		return -1;
	}
	
	m_size = size ;
	memset(m_buf, 0, sizeof(void*)*size);
	return 0;
}

int ring_buffer::push(void* data)
{
	if(!m_size)
	{
		return -1;
	}
	
	auto_lock __lock(m_w_mutex);
	if(m_buf[m_write])
	{
		return -1;
	}
	m_buf[m_write] = data;
		
	if(++m_write == m_size)
	{
		m_write = 0;
	}

	sem_post(&m_hsem);
	
	return 0;
}

int ring_buffer::pop(void *&data, int msecs)
{
	if(!m_size)
	{
		return -1;
	}
	
	if(msecs >= 0)
	{
		struct timeval now;
		struct timespec abstime;
		gettimeofday(&now, NULL);

		abstime.tv_sec = now.tv_sec + msecs / 1000;
		abstime.tv_nsec = 1000 * (now.tv_usec + (msecs % 1000) * 1000);
		const long BILLION = 1000000000;
		if (abstime.tv_nsec >= BILLION)
		{
			abstime.tv_sec++;
			abstime.tv_nsec -= BILLION;
		}

		if(sem_timedwait(&m_hsem, &abstime) == -1)
		{
			printf_t("error: sem_timedwait fail %d\n", errno);
			return -1;
		}
	}
	else
	{
		if(sem_wait(&m_hsem) == -1)
		{
			printf_t("error: sem_wait fail %d\n", errno);
			return -1;
		}
	}
	
	data = m_buf[m_read];
	if(!data)
	{
		return -1;
	}

	m_buf[m_read] = 0;
	
	if(++m_read == m_size)
	{
		m_read = 0;
	}
	

	return 0;
}
/********************************************************/



struct thread_template{
	thread_fun fun;
	void * param;
	char  name[max_app_name + 1];
};


void *thread_task(void *param){
	thread_template * t = (thread_template*)param;
	prctl(PR_SET_NAME, t->name);

	thread_fun fun = t->fun;
	void * p = t->param;
	delete t;
	
	(*fun)(p);
	return 0;
}

int create_thread(pthread_t & tid, thread_fun fun, const char * name, void * param)
{

	//struct sched_param tparam;	
	//uint stack_size = 512<<10;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
    //pthread_attr_setstacksize(&attr, stack_size);
    //pthread_attr_setscope(&atrr, PTHREAD_SCOPE_SYSTEM)
    //pthread_attr_setschedpolicy(&attr, SCHED_OTHER)
    //pthread_attr_getschedparam(&attr, tparam)
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	//for thread name, add 2014-8-28
	thread_template * t = new thread_template;
	t->param = param;
	t->fun  = fun;
	int len = sizeof(t->name) -1;
	strncpy(t->name, name, len);
	t->name[len] = 0;
		
	if(pthread_create(&tid, &attr, thread_task, t)){
		pthread_attr_destroy(&attr);
		delete t; //add by 2014/10/3
		return -1;
	}
	
	pthread_attr_destroy(&attr);
	
	return 0;
	
}


int make_no_block(int fd)
{
	int nFlags = fcntl(fd, F_GETFL, 0);
	if (nFlags < 0){
		printf_t("error: fcnt getfl error\n");
		return -1;
	}
	
	if (fcntl(fd, F_SETFL, nFlags|O_NONBLOCK) < 0){
		printf_t("error: fcnt setfl error\n");
		return -1;
	}
	
	return 0;
}

int64 get_tick_count()
{
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	return ((int64)tv.tv_sec * 1000) + tv.tv_usec/1000;
}


int create_tcp_listen(short port, int reuse, int blog)
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);	
	if(listenfd < 0)
	{
		printf_t("error: create listen socket error\n");
		return -1;
	}
	
	
	if (make_no_block(listenfd) < 0){
		printf_t("error: fcnt getfl error\n");
		close(listenfd);
		return -1;
	}
	
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
	{
		printf_t("error: setsockopt SO_REUSEADDR %d\n", errno);
		close(listenfd);
		return -1;
	}
	
	// bind & listen    
	sockaddr_in sin ;         
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;
	
	if(bind(listenfd, (const sockaddr*)&sin, sizeof(sin)) < 0)
	{
		printf_t("error: socket bind error %d\n", errno);
		close(listenfd);
		return -1;
	}
	
	if(listen(listenfd, blog) < 0)
	{
		printf_t("error: listen error %d\n", errno);
		close(listenfd);
		return -1;
	}

	return listenfd;
}

int printf_t(const char * format,...)
{
	char ach_msg[1024] = {0};
	
	time_t cur_t = time(NULL);
    struct tm cur_tm ;
	memcpy(&cur_tm, localtime(&cur_t), sizeof(cur_tm));
	
	sprintf(ach_msg, 
			"%d-%02d-%02d %02d:%02d:%02d ",
			 cur_tm.tm_year + 1900, 
			 cur_tm.tm_mon + 1,
			 cur_tm.tm_mday,
			 cur_tm.tm_hour,
			 cur_tm.tm_min, 
			 cur_tm.tm_sec);
			 
	va_list pv_list;
    va_start(pv_list, format);	
    vsprintf(ach_msg + strlen(ach_msg), format, pv_list); 
    va_end(pv_list);
	
	ach_msg[1024 -1] = 0;
	
	printf(ach_msg);
	fflush(stdout);
	
	return 0;
}


