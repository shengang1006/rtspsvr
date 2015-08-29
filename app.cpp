#include "app.h"
#include "server.h"
#include "stdio.h"
#include "log.h"
/*app class*/

app::app()
{
	m_brun = false;
	m_drop_msg = 0;
	m_appid = 0;
	memset(m_name, 0, sizeof(m_name));
}

app::~app()
{
	m_brun = false;
	m_drop_msg = 0;
}

void * app::app_run(void* param)
{
	app * s = (app*)param;
	s->run();
	return 0;
}
	
int app::run()
{
	while(m_brun)
	{
		void* data = NULL;
		if(m_ring_buf.pop(data) == -1)
		{
			continue;
		}
		
		hd_app * msg = (hd_app*)data;
		
		if(msg->type == tcp_type)
		{
			switch(msg->event){
				case ev_recv:
					on_recv(msg->u.tcp.n, msg->content, msg->length);
					break;
					
				case ev_close:
					on_close(msg->u.tcp.n, *((int*)msg->content));
					break;
					
				case ev_accept:
					on_accept(msg->u.tcp.n);
					break;
					
				case ev_connect_ok:
				case ev_connect_fail:
					on_connect(msg->u.tcp.n);
				break;
			}
			msg->u.tcp.n->release_ref();
		}
		else if(msg->type == timer_type)
		{
			on_timer(msg->event, msg->u.timer.interval, msg->u.timer.ptr);
		}
		else if(msg->type == app_type)
		{
			on_app(msg->event, msg->content, msg->length);
		}
		else
		{
			printf_t("error: msg from unknown app_name(%s)\n", m_name);
		}
	
		free(msg);
	}
	return 0;
}

int app::create(int appid, int msg_count, const char * app_name)
{
	if(m_ring_buf.create(msg_count) == -1)
	{
		printf_t("error: create ring buffer fail\n");
		return -1;
	}
	
	strncpy(m_name, app_name, sizeof(m_name)-1);
	
	m_brun = true;
	pthread_t tid;
	if(create_thread(tid, app_run, m_name, (void*)this) == -1)
	{
		printf_t("error: create_thread fail\n");
		return -1;
	}
	
	m_appid = appid;
	return 0;
}

int app::get_appid()
{
	return m_appid;
}

int  app::push(hd_app * msg)
{
	connection * n  = NULL;
	if(msg->type == tcp_type)
	{
		n = msg->u.tcp.n;
		n->add_ref();
	}
	
	int res =  m_ring_buf.push(msg);
	
	if(res < 0 && n)
	{
		n->release_ref();
	}

	return res;
}

int app::increase_drop_msg()
{
	return ++m_drop_msg;
}

const char * app::name()
{
	return m_name;
}

int app::add_timer(int id, int interval, void * context)
{
	return server::instance()->add_timer(id, interval, m_appid, context);
}


int app::add_abs_timer(int id, int year, int mon, int day, 
					  int hour, int min, int sec, void * context /* = NULL */){

	return server::instance()->add_abs_timer(id, year, mon, day, hour, min, sec, m_appid, context);
}

int app::post_connect(const char * ip, ushort port, int delay, void * context)
{
	return server::instance()->post_connect(ip, port, delay, m_appid, context);
}
	
int app::post_app_msg(int dst, int event, void * content, int length)
{
	return server::instance()->post_app_msg(dst, event, content, length);
}

int app::log_out(int lev, const char * format,...)
{

	char ach_msg[max_log_len] = {0};
	time_t cur = time(NULL);
	struct tm cur_tm ;
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
			strcpy(pos, "[unknown]");
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

