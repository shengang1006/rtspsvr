#include "app.h"
#include "server.h"

app::app(){
	m_brun = false;
	m_drop_msg = 0;
	m_appid = 0;
	memset(m_name, 0, sizeof(m_name));
}

app::~app(){
	m_brun = false;
	m_drop_msg = 0;
}

void * app::app_run(void* param){
	app * s = (app*)param;
	s->run();
	return 0;
}
	
int app::run(){
	
	while(m_brun){
		
		void* data = NULL;
		if(m_ring_buf.pop(data) == -1){
			continue;
		}
		
		hd_app * msg = (hd_app*)data;
		if(msg->type == tcp_type){
			switch(msg->event){
				case ev_recv:
					on_recv(msg->u.tcp.n, msg->content, msg->length);
					break;
					
				case ev_close:
					on_close(msg->u.tcp.n, *((int*)msg->content));
					delete msg->u.tcp.n;
					break;
					
				case ev_accept:
					on_accept(msg->u.tcp.n);
					break;
					
				case ev_connect_ok:
					on_connect(msg->u.tcp.n);
					break;
					
				case ev_connect_fail:
					on_connect(msg->u.tcp.n);
					delete msg->u.tcp.n;
				break;
				
			}
		}
		else if(msg->type == timer_type){
			on_timer(msg->event, msg->u.timer.interval, msg->u.timer.ptr);
		}
		else if(msg->type == app_type){
			on_app(msg->event, msg->content, msg->length);
		}
		else{
			error_log("msg from unknown app_name(%s)\n", m_name);
		}
		free(msg);
	}
	return 0;
}

int app::create(int appid, int msg_count, const char * app_name){
	
	if(m_ring_buf.create(msg_count) == -1){
		error_log("create ring buffer fail\n");
		return -1;
	}
	
	strncpy(m_name, app_name, sizeof(m_name)-1);
	
	m_brun = true;
	pthread_t tid;
	if(create_thread(tid, app_run, m_name, (void*)this) == -1){
		error_log("create_thread fail\n");
		return -1;
	}
	
	m_appid = appid;
	return 0;
}

int app::get_appid(){
	return m_appid;
}

int  app::push(const hd_app & temp){
	
	hd_app * msg = (hd_app*)malloc(sizeof(hd_app) + temp.length + 1);
	if(!msg){
		error_log("malloc %d bytes fail, error(%d)\n", sizeof(hd_app) + temp.length + 1, errno);
		return -1;
	}
	memcpy(msg, &temp, sizeof(hd_app));
	
	if(temp.length){
		msg->content = (char*)msg + sizeof(hd_app) ;
		memcpy(msg->content, temp.content, temp.length);
		msg->content[temp.length] = 0;	
	}
	
	if(m_ring_buf.push(msg) < 0){
		error_log("drop msg total(%d)\n", ++m_drop_msg);
		free(msg);
		return -1;
	}
	return 0;
}

const char * app::name(){
	return m_name;
}

int app::add_timer(int id, int interval, void * context){
	return server::instance()->add_timer(id, interval, m_appid, context);
}


int app::add_abs_timer(int id, int year, int mon, int day, 
					  int hour, int min, int sec, void * context /* = NULL */){

	return server::instance()->add_abs_timer(id, year, mon, day, hour, min, sec, m_appid, context);
}

int app::post_connect(const char * ip, ushort port, int delay, void * context){
	return server::instance()->post_connect(ip, port, delay, m_appid, context);
}
	
int app::post_app_msg(int dst, int event, void * content, int length){
	return server::instance()->post_app_msg(dst, event, content, length);
}