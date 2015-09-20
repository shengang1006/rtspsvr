#include <sys/resource.h>
#include <signal.h> 
#include "watcher.h"

watcher::watcher(){
	m_listenfd = -1;
	m_port = 0;
	m_worker_number = 0;
	memset(m_workers, 0, sizeof(m_workers));
}

watcher::~watcher(){
	if(m_listenfd > 0){
		close(m_listenfd);
		m_listenfd = -1;
	}
}

watcher * watcher::instance(){
	static watcher inst;
	return &inst;
}

int watcher::init(){
	//resource limit set 2015-1-23
	struct rlimit limit;
	limit.rlim_cur = 60000;
	limit.rlim_max = 60000;
	if(setrlimit(RLIMIT_NOFILE, &limit) < 0){
		warn_log("setrlimit error(%d)\n", errno);
	}
	
	//ignore socket pipe 2015-1-23
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if(sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) < 0) { 
		warn_log("sigaction error(%d)\n", errno);
	}
	
	//ignore thread pipe 2015-1-23
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) < 0){
		warn_log("pthread_sigmask error(%d)\n", errno);
	}

	return 0;
}

int watcher::create_tcp_server(ushort port, int reuse /* = 1 */){

	//create noblock tcp listen socket
	if(m_listenfd >= 0){
		error_log("listen socket already create\n");
		return -1;
	}

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);	
	if(listenfd < 0){
		error_log("socket error(%d)\n", errno);
		return -1;
	}
	
	if (make_no_block(listenfd) < 0){
		error_log("make_no_block error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0){
		error_log("setsockopt SO_REUSEADDR error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	// bind & listen    
	sockaddr_in sin ;         
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;
	
	if(bind(listenfd, (const sockaddr*)&sin, sizeof(sin)) < 0){
		error_log("bind error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	
	if(listen(listenfd, SOMAXCONN) < 0){
		error_log("listen error(%d)\n", errno);
		close(listenfd);
		return -1;
	}
	debug_log("listen SOMAXCONN = %d\n", SOMAXCONN);

	m_listenfd = listenfd;
	m_port     = port;
	return 0;
}

int watcher::tcp_port(){
	return m_port;
}

int watcher::reg_worker(worker * s){
	
	if(m_worker_number >= max_worker_number){
		error_log("too much worker\n");
		return -1;
	}
	if(s->init() < 0){
		error_log("worker init fail\n");
		return -1;
	}
	m_workers[m_worker_number++] = s;
	return 0;
}


void * watcher::worker_task(void* param){
	
	worker * s = (worker*)param;
	s->run();
	return 0;
}

int watcher::loop(){

	prctl(PR_SET_NAME, "watcher_task");
	
	for(int k = 0; k < m_worker_number ; k++){
		if (m_listenfd > 0){
			m_workers[k]->register_listen(m_listenfd);
		}
		create_thread(NULL, worker_task, "worker_task", m_workers[k]);
	}
	
	for(;;){
		sleep(1);
	}
	return 0;
}