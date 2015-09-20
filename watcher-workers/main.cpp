#include "watcher.h"
#include <string.h>
#include <list>

#include <arpa/inet.h>
#include <netdb.h>
//#include "cJSON.h"
#include "log.h"

#define ev_sys_timer_keepalive  ev_sys_timer_user + 1

struct header
{
	int tag;
	int len;
	int event;
	int ver;
};

enum log_level{
	log_info = 0, 
	log_debug, 
	log_warn, 
	log_error
};

#define  MAGIC_TAG 0xFFEEDDCC
#define  count_per 10
class mysvr:public worker
{
public:
	mysvr(int id){
		m_id = id;
	}

	int log_out(int lev, const char * format,...){
		
		char ach_msg[max_log_len] = {0};
		
		struct tm cur_tm;
		time_t cur = time(NULL);
		localtime_r(&cur, &cur_tm);
		
		sprintf(ach_msg, 
			"[%d-%02d-%02d %02d:%02d:%02d]",
			cur_tm.tm_year + 1900, 
			cur_tm.tm_mon + 1,
			cur_tm.tm_mday,
			cur_tm.tm_hour,
			cur_tm.tm_min, 
			cur_tm.tm_sec);
		char * pos = ach_msg + strlen(ach_msg);
		
		switch(lev)
		{	
		case log_error: 
			strcpy(pos, "[error]");
			break;
		case log_warn : 
			strcpy(pos, "[warn]");
			break;
		case log_debug: 
			strcpy(pos, "[debug]");
			break;
		case log_info:
			strcpy(pos, "[info]");
			break;
		default:
			break;
		}
		pos += strlen(pos);
		
		int nsize = max_log_len - (int)(pos - ach_msg);
		
		va_list pv_list;
		va_start(pv_list, format);	
		vsnprintf(pos, nsize, format, pv_list); 
		va_end(pv_list);
		
		return log::instance()->write_log(ach_msg);
	}

	int on_initialize(){
//		for(int k = 0; k < 2000; k++)
		post_connect("192.168.19.219", 29027, 1000, NULL);
	//	set_timer(ev_sys_timer_keepalive, 5000);
		set_timer(ev_sys_timer_keepalive + 1, 10*1000);
		return 0;
	}

	int on_accept(connection * n){
		ipaddr & peer = n->get_peeraddr();
		debug_log( "thread(%u) on_accept client(%s:%d)\n", pthread_self(), peer.ip, peer.port);
	//	m_listcon.push_back(n);
		
		return 0;
	}

	int on_recv(connection * n, char * data, int len){
		debug_log("on_recv:%s\n", data);
		return 0;
	}


	int on_close(connection * n, int reason){
		ipaddr & peer = n->get_peeraddr();
		debug_log( "on_close client(%s:%d) reason %d\n", peer.ip, peer.port, reason);
		post_connect(peer.ip, peer.port, 10000, n->get_context());
		m_listcon.remove(n);
		return 0;
	}

	int login_dev(connection * n){
		char * data = "{\"cmd\":\"add_screentshot_plan_req\",\"serial\":\"500\",\"data\":{\"device_list\":["
				"{\"id\":10012,\"chn\":0},{\"id\":10009,\"chn\":0}],\"mode\":\"every_day\",\"every_day\":[{\"hour\":13,\"min\":30},{\"hour\":14,\"min\":0}]}}\r\n\r\n";
	//	n->post_send(data, strlen(data));
		return 0;
	}

	int on_connect(connection * n){
		ipaddr & svr = n->get_peeraddr();
		if(!n->connected()){
			post_connect(svr.ip, svr.port, 5000, n->get_context());
			log_out(log_debug, "on_connect client(%s:%d) fail\n", svr.ip, svr.port);
		}else{
			log_out(log_debug, "on_connect client(%s:%d) ok\n", svr.ip, svr.port);
	//		disconnect(n);
		//	post_connect(svr.ip, svr.port, 5000, n->get_context());
			m_listcon.push_back(n);
			login_dev(n);
		}
		return 0;
	}

	int on_timer(int event, int interval, void * ptr){
		char * data = NULL;
		debug_log("%lld timer %d\n", get_tick_count(), event);
		set_timer(event, interval, ptr);

		return 0;
		std::list<connection*>::iterator it = m_listcon.begin();
		for(; it != m_listcon.end(); it++){

	
			data = "{\"cmd\":\"stop_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
	//		sleep(20);


			data = "{\"cmd\":\"direction_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0,\"direction\":0,\"speed\":4}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
//			sleep(1);
			data = "{\"cmd\":\"stop_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
	//		sleep(20);

			data = "{\"cmd\":\"direction_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0,\"direction\":1,\"speed\":4}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
	//		sleep(1);
			data = "{\"cmd\":\"stop_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
	//		sleep(20);

			data = "{\"cmd\":\"direction_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0,\"direction\":2,\"speed\":4}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
	//		sleep(1);
			data = "{\"cmd\":\"stop_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
		//	sleep(20);

			data = "{\"cmd\":\"direction_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0,\"direction\":3,\"speed\":4}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
		//	sleep(1);
			data = "{\"cmd\":\"stop_ctrl_req\",\"serial\":\"0\",\"data\":{\"device_id\":10016,\"channel\":0}}\r\n\r\n";
			(*it)->post_send(data, strlen(data));
		//	sleep(20);
		}
		debug_log("timer %d\n", event);
		set_timer(event, interval, ptr);
		return 0;
	}
	

	int on_unpack(char * data, int len, int & packlen,char *&packet){

	

		if(len > 1024 * 64){
			return -1;
		}
		
		char * flag = strstr(data, "\r\n\r\n");
		if(flag){
			*flag = 0;
			packet = data;
			packlen = flag - data;
			return packlen + 4;
		}
		else{
			return 0;
		}
	}


private:
	std::list<connection*>m_listcon;
	int m_id;
};

//#include<sys/types.h> 
#include <sys/stat.h>
#include <sys/param.h>
#include "utility.h"
void* test(void* arg)
{

	pthread_detach(pthread_self());
	return NULL;
}
int main(int argc, char *argv[])
{
	
	int64 starttm = get_tick_count();
	
	auto_mutex m_mutext; 
	int m = 0;
	for(int k = 0; k < 1000000; k++){
		//auto_lock lock (m_mutext);
	//	__sync_fetch_and_add(&m_ref, 1);
		m++;
	}
	
	printf("take :%ld\n", get_tick_count() - starttm);

	//sleep(10000);
	//return 1;

	debug_log("mysvr init %d\n", sizeof(int64));
	
	watcher * s = watcher::instance();
	s->init();
	s->create_tcp_server(4000);
	log::instance()->init("./log", "reactor");
	
	
	for(int k = 0; k< 1; k++){
		mysvr * svr = new mysvr(k);
		//svr->set_keepalive(-1);
		s->reg_worker(svr);
	}
	s->loop();
	return 0;
}