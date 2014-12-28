#include "app.h"
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
			printf_t("msg from unknown\n");
		}
	
		free(msg);
	}
	return 0;
}

int app::create(int appid, int msg_count, const char * app_name, int app_mode)
{
	if(m_ring_buf.create(msg_count) == -1)
	{
		printf_t("error : create ring buffer fail\n");
		return -1;
	}
	
	strncpy(m_name, app_name, sizeof(m_name)-1);
	
	m_brun = true;
	pthread_t tid;
	if(create_thread(tid, app_run, m_name, (void*)this) == -1)
	{
		printf_t("error : create_thread fail\n");
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



