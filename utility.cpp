#include "utility.h"
#include <unistd.h>
#include <fcntl.h>
#include<sys/stat.h>

/********************************************************/
auto_mutex::auto_mutex(){
	pthread_mutex_init(&m_mutex, NULL);
}

auto_mutex::~auto_mutex(){
	pthread_mutex_destroy(&m_mutex);
}

void auto_mutex::lock(){
	pthread_mutex_lock(&m_mutex);
}

void auto_mutex::unlock(){
	pthread_mutex_unlock(&m_mutex);
}


auto_lock::auto_lock(auto_mutex & mutex)
	:m_mutex(mutex){
	m_mutex.lock();
}
auto_lock::~auto_lock(){
	m_mutex.unlock();
}


/********************************************************/
ring_buffer::ring_buffer(){
	m_buf = NULL;
	m_size = 0;
	m_read = 0;
	m_write = 0;
}

ring_buffer::~ring_buffer(){
	
	if(m_buf){
		free(m_buf);
		m_buf = NULL;
	}
	
	if(sem_destroy(&m_hsem) == -1){
		error_log("sem_destroy error(%d)\n", errno);
	}
	m_size = 0;
	m_read = 0;
	m_write = 0;
}

int ring_buffer::create(int size){	

	if(m_buf){
		return -1;
	}
	
	if(sem_init(&m_hsem, 0, 0) == -1){
		error_log("sem_init error(%d)\n", errno);
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

int ring_buffer::push(void* data){
	
	if(!m_size){
		return -1;
	}
	
	if(m_buf[m_write]){
		return -1;
	}
	m_buf[m_write] = data;	
	if(++m_write == m_size){
		m_write = 0;
	}
	
	sem_post(&m_hsem);
	
	return 0;
}

int ring_buffer::pop(void *&data, int msecs){
	
	int ret = 0;
	if(msecs >= 0){
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

		while ((ret = sem_timedwait(&m_hsem, &abstime)) < 0 && errno == EINTR)
			continue;
	}
	else{

		while ((ret = sem_wait(&m_hsem)) < 0 && errno == EINTR)
			continue;
	}
	
				
	if(ret < 0){
		if(errno != ETIMEDOUT){
			error_log("sem_timedwait/sem_wait error(%d)\n", errno);
		}	
		return -1;
	}

	if(!m_size){
		return -1;
	}
	
	data = m_buf[m_read];
	if(!data){
		error_log("fatal error data is null\n");
		return -1;
	}

	m_buf[m_read] = 0;
	if(++m_read == m_size){
		m_read = 0;
	}
	return 0;
}
/********************************************************/



struct thread_template{
	thread_fun fun;
	void * param;
	char  name[max_app_name];
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

int create_thread(pthread_t * tid, thread_fun fun, const char * name, void * param){

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

	pthread_t th = 0;
	if(pthread_create(&th, &attr, thread_task, t)){
		pthread_attr_destroy(&attr);
		delete t; //add by 2014/10/3
		return -1;
	}
	
	if(tid){
		*tid = th;
	}
	
	pthread_attr_destroy(&attr);
	return 0;
}

int create_directory(const char * path, int amode /*= 777*/){

	int len = strlen(path);
	if(len == 0 || len >= 512){
		return -1;
	}

	char dirname[512] = {0};
	strcpy(dirname, path);
	if(dirname[len -1] != '/'){
		dirname[len++] = '/' ;
	}


	for (int i = 1; i < len; i++){
		if(dirname[i] == '/'){
			dirname[i] = 0;
			if (access(dirname, 0)){
				if (mkdir(dirname, amode) < 0){
					error_log("mkdir error(%d)\n", errno);
					return -1;
				}
			}
			dirname[i] = '/';
		}
	}

	return 0;
}

int make_no_block(int fd)
{
	int nFlags = fcntl(fd, F_GETFL, 0);
	if (nFlags < 0){
		error_log("fcnt getfl error(%d)\n", errno);
		return -1;
	}
	
	if (fcntl(fd, F_SETFL, nFlags|O_NONBLOCK) < 0){
		error_log("fcnt setfl error(%d)\n", errno);
		return -1;
	}
	
	return 0;
}

int64 get_tick_count(){
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	return ((int64)tv.tv_sec * 1000) + tv.tv_usec/1000;
}

int sys_log(FILE * fd, const char *format, ...){
	
	char buf[1024] = {0};
	
	va_list ap;
    va_start(ap, format);	
    vsnprintf(buf, sizeof(buf), format, ap);  
    va_end(ap);
	
	struct tm tm;
	time_t now = time(NULL);
	localtime_r(&now, &tm);
	
	//Aug 01 11:28:04 4590 main[370]: error:
	char tmpbuf[128] = {0};
	strftime(tmpbuf, sizeof(tmpbuf), "%b %d %X", &tm);
  
	fprintf(fd, "%s %d %s", tmpbuf, getpid(), buf);
	return 0;
}

