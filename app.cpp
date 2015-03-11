#include "app.h"
#include "server.h"
#include "stdio.h"
/*app class*/

app::app()
{
	m_brun = false;
	m_drop_msg = 0;
	m_appid = 0;
	m_mode = app_shared;
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
		
		app_msg * msg = (app_msg*)data;
		
		if(msg->from == from_net)
		{
			connection * n = (connection *)msg->ptr;
			on_net(n, msg->event, msg->content, msg->length);
			n->release_ref();
		}
		else if(msg->from == from_timer)
		{
			on_timer(msg->event, msg->ptr);
		}
		else if(msg->from == from_app)
		{
			on_app(msg->event, msg->content, msg->length, msg->src);
		}
		else
		{
			printf_t("error: msg from unknown app_name(%s)\n", m_name);
		}
	
		free(msg);
	}
	return 0;
}

int app::create(int appid, int msg_count, const char * app_name, int app_mode)
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
	m_mode = app_mode;
	return 0;
}

int app::get_appid()
{
	return m_appid;
}

int app::get_app_mode()
{
	return m_mode;
}

int  app::push(app_msg * msg)
{
	connection * n  = NULL;
	if(msg->from == from_net)
	{
		n = (connection *)msg->ptr;
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

int app::post_connect(const char * ip, short port, int delay, void * context)
{
	return server::instance()->post_connect(ip, port, delay, m_appid, context);
}
	
int app::post_app_msg(int dst, int event, void * content, int length, int src)
{
	return server::instance()->post_app_msg(dst, event, content, length, src);
}

int app::log_out(int lev, const char * format,...)
{
	if(lev < server::instance()->get_loglev()){
		return 0;
	}
	
	char ach_msg[max_log_len] = {0};
	
	time_t cur_t = time(NULL);
    struct tm cur_tm = *localtime(&cur_t);
	
	if(lev != log_none)
	{
		sprintf(ach_msg, 
			"[%d-%02d-%02d %02d:%02d:%02d]",
			 cur_tm.tm_year + 1900, 
			 cur_tm.tm_mon + 1,
			 cur_tm.tm_mday,
			 cur_tm.tm_hour,
			 cur_tm.tm_min, 
			 cur_tm.tm_sec);
    }
	
	char * pos = ach_msg + strlen(ach_msg);
	
	int color = color_green;
	switch(lev)
	{	
		case log_error: 
			color = color_red;
			strcpy(pos, "[error]");
		break;
		case log_warn : 
			color = color_yellow; 
			strcpy(pos, "[warn]");
		break;
		case log_debug: 
			color = color_green;
			strcpy(pos, "[debug]");
		break;
		case log_info:
			color = color_white;
			strcpy(pos, "[info]");
		break;
		default:
			color = color_purple;
		break;
	}
	pos += strlen(pos);
	
    int nsize = max_log_len - (int)(pos - ach_msg);
		 	
    va_list pv_list;
    va_start(pv_list, format);	
    vsnprintf(pos, nsize, format, pv_list); 
    va_end(pv_list);
	
	return server::instance()->log_out(lev, color, ach_msg);
}

