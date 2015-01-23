#include "log.h"
#include <sys/epoll.h>

static const char * color_table[] = 
{
	"\033[0;30m" , "\033[0;31m" , "\033[1;31m" , "\033[0;32m",
	"\033[1;32m" , "\033[0;34m" , "\033[1;34m" , "\033[1;30m",
	"\033[0;36m" , "\033[1;36m" , "\033[0;35m" , "\033[1;35m",
	"\033[0;33m" , "\033[1;33m" , "\033[0;37m" , "\033[1;37m",
	"\033[m",
};

static char backspace_cmd[]= 
{
	backspace_char,
	blank_char,
	backspace_char,
	0,
};


telnet::telnet()
{
	m_listenfd = -1;
	m_clientfd = -1;
	m_brun = false;
	memset(m_curcmd, 0, sizeof(m_curcmd));
	memset(&m_cmd_history, 0, sizeof(m_cmd_history));
	memset(m_cmd_table, 0, sizeof(m_cmd_table));
	m_regs = 0;
	m_stop_echo = true;
	m_epfd = -1;
}
	
telnet::~telnet()
{
	m_brun = false;
	
	if(m_clientfd >=0)
	{
		close(m_clientfd);
		m_clientfd = -1;
	}
	
	if(m_listenfd >=0)
	{
		close(m_listenfd);
		m_listenfd = -1;
	}
	
	if(m_epfd >= 0)
	{
		close(m_epfd);
		m_epfd = -1;
	}
}

int telnet::send_nvt_cmd(char cmd, char opt)
{
	char buf[3] = {0};
	buf[0] = telcmd_iac;
	buf[1] = cmd;
    buf[2] = opt;
	lg_send((const char*)buf, sizeof(buf));
	return 0;
}

int telnet::lg_send(const char * msg, int len)
{
	int msg_len = len;
	int total_sent = 0;

	while(total_sent  < msg_len)
	{
		int res = send(m_clientfd, (char*)msg + total_sent, msg_len - total_sent, 0);
		if(res < 0 && (errno != EINTR && errno != EAGAIN))
		{
			printf_t("error: lg_send fail socket(%d) errno(%d)\n", m_clientfd, errno);
			lg_close();
			break;
		}
		total_sent += res;
	}
	
	return 0;
}

int telnet::echo(const char * msg, int color)
{
	if(m_stop_echo || m_clientfd == -1)
	{
		return 0;
	}
	
	int count = sizeof(color_table)/sizeof(color_table[0]);
	color = color >= count ? 0 : color;
	
	//add color mod
	char out_buf[max_log_len + 512] = {0};
	strcat(out_buf, color_table[color]);
	
	//replace '\n' '\r\n'
	char * temp = out_buf +  strlen(color_table[color]);
	
	while(*msg)
	{
		*temp = *msg;
		if (*msg == '\n')
		{
			*temp++ = '\r';
			*temp   = '\n';
		}
		msg ++;
		temp ++;
	}
		
	//recover color mode
	strcat(temp, color_table[color_none]);
	
	int msg_len = temp + strlen(color_table[color_none]) - out_buf + 1;
	lg_send((char*)out_buf, msg_len);
	
	return 0;
}
	
	
void * telnet::telnet_task(void * param)
{
	telnet * t = (telnet*)param;
	t->lg_run();
	return 0;
}


int telnet::lg_run()
{
	
	struct epoll_event events[16]; 
	while(m_brun)
	{
		int res = epoll_wait (m_epfd, events, 16, -1);	
		if (res == -1)
		{
			printf_t("error: epoll_wait error(%d %s)\n", errno, strerror(errno));
			if (errno != EINTR) 
			{
				break;
			}
			continue;
		}
		
		for(int i = 0; i < res; i++)
		{
			//accept
			if (events[i].data.fd == m_listenfd)
			{	
				lg_accept();
				continue;
			}
			
			if (events[i].events & EPOLLIN)
			{
				lg_recv();
			}
		}
	}
	
	return 0;
}

int telnet::lg_accept()
{
	if(m_clientfd != -1)
	{
		printf_t("warn : more than one user login\n");
		lg_close();
	}

	struct sockaddr_in clientaddr;
    int  addrlen = sizeof(clientaddr);
	m_clientfd = accept(m_listenfd,(struct sockaddr *)&clientaddr, (socklen_t*)&addrlen);
	
	if (m_clientfd == -1)
	{
		printf_t("error: socket accept error(%d)\n", errno);
		return -1;
	}
	
	if(make_no_block(m_clientfd) < 0)
	{
		printf_t("error: make_no_block error(%d)\n", errno);
		close(m_clientfd);
		m_clientfd = -1;
		return -1;
	}
	
	struct epoll_event ev;
	ev.data.fd = m_clientfd;
	ev.events =  EPOLLIN;
	if (epoll_ctl (m_epfd, EPOLL_CTL_ADD, m_clientfd, &ev) < 0) {
		printf_t("error: EPOLL_CTL_ADD fail(%d)\n", errno);
		return -1;
	}
		
	m_stop_echo = false;
	send_nvt_cmd(telcmd_do, telopt_echo);//开启行模式
	send_nvt_cmd(telcmd_do, telopt_naws);//开启行模式
	send_nvt_cmd(telcmd_do, telopt_lflow);//远端流量控制
	send_nvt_cmd(telcmd_will, telopt_echo);
	send_nvt_cmd(telcmd_will, telopt_sga);
	
	echo("/**************************************/\n"
		 "/*欢迎使用TELNET服务器\n"
		 "/*TELNET-V1.0 2013/3/20\n"
		 "/**************************************/\n",
		color_green);
		
	return 0;
}

//fixed 2015-1-8
int telnet::lg_close()
{
	if(m_clientfd == -1){
		return 0;
	}
	struct epoll_event ev = {0};
	ev.data.ptr = (void*)this;
	epoll_ctl(m_epfd, EPOLL_CTL_DEL, m_clientfd, &ev);
	close(m_clientfd);
	m_clientfd = -1;
	return 0;
}


int telnet::lg_recv()
{
	char recv_buf[max_cmd_len] = {0};
	
	int recv_len = recv(m_clientfd, recv_buf, sizeof(recv_buf) -1, 0);
	if(recv_len <= 0)
	{
		lg_close();
		return -1;
	}

	//NVT
	if (recv_buf[0] == -1)
	{
		return 0;
	}

	switch (recv_buf[0])
	{
		case ctrl_s:break;
		case ctrl_r:break;
		case ctrl_c:
		{
			m_stop_echo = !m_stop_echo; 
			char info[128] = {0};
			strcat(info, color_table[color_cyan]);
			if(m_stop_echo)
			{
				strcat(info,"telnet print stopped\r\n");
			}
			else
			{
				strcat(info,"telnet print start\r\n");
			}
			strcat(info, color_table[color_none]);
			lg_send(info, strlen(info));
		}
		break;
		case tab_char:break;
		case return_char:
		{	
			
			if(!strlen(m_curcmd))
			{
				lg_send(m_prompt, strlen(m_prompt));
				break;
			}
			if(!run_cmd(m_curcmd))
			{
				lg_send(m_prompt, strlen(m_prompt));
			}
			else
			{
				char err_rsp[128] = {0};
				sprintf(err_rsp, "\r\n%serror: cmd not exist%s%s",color_table[color_red], color_table[color_none], m_prompt);
				lg_send(err_rsp, strlen(err_rsp));
			}
			set_cmd(m_curcmd);
			memset(m_curcmd, 0, sizeof(m_curcmd));
		}
		break;
		case backspace_char:
		{
			int len = strlen(m_curcmd);
			if (len >0 )
			{				
				m_curcmd[len - 1] = 0;
				lg_send(backspace_cmd, sizeof(backspace_cmd));
			}
		}			
		break;
		case arrow_first:	
		{
			if (recv_buf[1] == arrow_second)
			{
				int mode = 0;
				if(recv_buf[2] == arrow_up)
				{
					mode = -1;
				}
				if(recv_buf[2] == arrow_down)
				{
					mode = 1;
				}
				if(!mode)
				{
					break;
				}
				
				char * last_cmd = next_cmd(mode);
				if (last_cmd)
				{
					lg_replace(last_cmd, m_curcmd);
				}
			}
		}
		break;
		default:
		{
			int len = strlen(m_curcmd);
			for (int n = 0; n < recv_len ; n++)
			{
				if (len + 1 < max_cmd_len)
				{
					m_curcmd[len++] = recv_buf[n]; 
				}
				else
				{
					recv_buf[n] = 0;
					break;
				}
			}
			lg_send(recv_buf, strlen(recv_buf));//回显示
		}
		break;
	}	
	
	return 0;
}

int telnet::lg_replace(char * last, char * cur)
{
	int nlast = strlen(last);
	int ncur  = strlen(cur);
	char my_msg [max_cmd_len * 3] = {0};
	for (int it = 0; it < ncur ;it ++)
	{
		my_msg[it] = backspace_char;
	}
	memcpy(my_msg + ncur, last, nlast);	
	int npos = nlast + ncur;
	if (ncur > nlast)
	{
		//多余部分填写空格
		for (int it = 0 ; it < ncur - nlast ; it++)
		{
			my_msg[npos + it] = blank_char;
			my_msg[npos + it + ncur - nlast ] = backspace_char;
		}
	}
	lg_send(my_msg, strlen(my_msg));
	memcpy(cur, last, max_cmd_len);
	
	return 0;
}

void telnet::set_cmd(char * pcmd)
{
	int pos = m_cmd_history.pos;
	if (strcmp(pcmd, m_cmd_history.vec_cmd[pos]))
	{	
		int tail = m_cmd_history.tail;
		memcpy(m_cmd_history.vec_cmd[tail], pcmd, max_cmd_len);
		tail++;
		tail = tail == max_cmd_history? 0: tail;
		m_cmd_history.pos =  tail;
		m_cmd_history.tail = tail;
	}
	
}
	
char* telnet::next_cmd(int mode)
{	
	int pos =  m_cmd_history.pos;
	mode = mode < 0 ? -1 : 1;
	pos = (pos + mode + max_cmd_history) % max_cmd_history;
	char * cmd =  m_cmd_history.vec_cmd[pos];
	int   len = strlen(cmd);
	if (len == 0)
	{
		pos = (pos - mode + max_cmd_history) % max_cmd_history;
		cmd =  m_cmd_history.vec_cmd[pos];
		len = strlen(cmd);
	}
	m_cmd_history.pos = pos;
	return len ? cmd : NULL;
}
	
int telnet::init(const char * prompt, short port)
{

	if(strlen(prompt) + 4 >= sizeof(m_prompt))
	{
		return -1;
	}
	
	m_listenfd = create_tcp_listen(port, 1, 10);
	if(m_listenfd < 0)
	{
		return -1;
	}

	m_epfd = epoll_create(8);
	if(m_epfd == -1)
	{
		return -1;
	}
	
	struct epoll_event ev;
	ev.data.fd = m_listenfd;
	ev.events =  EPOLLIN;
	if (epoll_ctl (m_epfd, EPOLL_CTL_ADD, m_listenfd, &ev) < 0) {
		return -1;
	}
	
	
	memset(m_prompt, 0, sizeof(m_prompt));
	sprintf(m_prompt, "\r\n%s->", prompt);
	
	m_brun = true;
	m_stop_echo = false;
	pthread_t tid;
	if(create_thread(tid, telnet_task, "telnet", this) < 0)
	{
		printf_t("error: create thread fail(%d)\n", errno);
		return -1;
	}
	return 0;
}

int telnet::reg_cmd(const char* name, void* func)
{
	if(m_regs < max_cmd_reg)
	{
		strncpy(m_cmd_table[m_regs].cmd, name, max_cmd_len -1);
		m_cmd_table[m_regs].fn = (bind_func)func;
		m_regs++;
		return 0;
	}
	return -1;
}

char*  next_item_pos(char * param, int &len, bool & digit)
{
	len = 0;
	//左边去掉空格
	while(*param && (*param) == ' ')
	{
		param++;
	}
	
	char * pstr = param;
	digit = true;
	
	//找到右边空格
	while(*param)
	{
		len ++;
		//标记一项结束
		if ((*param) == ' ')
		{
			*param++ = 0;
			break;
		}
		
		//判断该项字符串是否是数字
		if (*param < '0' || *param > '9')
		{
			digit = false;
		}
		param ++;
	}

	return pstr;
}


int telnet::run_cmd(char * pcmd)
{
	char temp[max_cmd_len] = {0};
	strcat(temp, pcmd);
	
	char cmd_name[max_cmd_len] = {0};
	char* params[10] = {0};
	
	char * pstr = temp;

	bool digit = false;
	int  nlen = 0;
	
	//得到函数名,判断是否是数字不合法
	pstr = next_item_pos(pstr, nlen, digit);
	memcpy(cmd_name, pstr, nlen);
	pstr += nlen;

	//得到参数
	for (int k = 0 ; k < 10 ;k ++)
	{
		pstr = next_item_pos(pstr, nlen, digit);
		if (digit)
		{
			params[k] = (char*)atoi(pstr);
		}
		else
		{
			params[k] = pstr;
		}
		pstr += nlen;
	}
		
	if(!strcmp(cmd_name, "bye"))
	{
		lg_close();
		return 0;
	}
	
	for(int k = 0; k < m_regs; k++)
	{	
		if(!strcmp(m_cmd_table[k].cmd, cmd_name))
		{	
			(*m_cmd_table[k].fn)(params[0],params[1],params[2],params[3],params[4],params[5],params[6],params[7],params[8],params[9]);
			return 0;
		}
	}
	
	return -1;
}


/************************************/
log::log()
{
	m_file = NULL;
	m_times = 0;
	memset(m_pathname, 0, sizeof(m_pathname));
	memset(m_filename, 0, sizeof(m_filename));
}

log::~log()
{
	if(m_file)
	{
		fclose(m_file);
		m_file = NULL;
	}
}


int log::init(const char* path, const char * name, int max_size)
{
	m_max_size = max_size <= (2<<10) ? (2<<10): max_size;
	if(path == NULL)
	{
		strcat(m_pathname, "./");
	}
	else
	{
		strcat(m_pathname, path);
	}
	
	if(m_pathname[strlen(m_pathname) -1] != '/')
	{
		strcat(m_pathname, "/");
	}
	
	strcat(m_pathname, name);
	
	return open_log();
}

int log::open_log()
{
	
	time_t cur = time(NULL);
	time(&cur);
	memcpy(&m_log_begin, localtime(&cur), sizeof(m_log_begin));
	m_log_end = m_log_begin;
	
	sprintf(m_filename,"%s-%d%02d%02d.log", 
		m_pathname, 
		m_log_begin.tm_year+1900, 
		m_log_begin.tm_mon+1,
		m_log_begin.tm_mday);
		
	m_file = fopen(m_filename, "a+");
	
	if(!m_file)
	{
		return -1;
	}
	
	fseek(m_file, 0L, SEEK_END);
	m_cur_size = (int)ftell(m_file);
	m_times = 0;
	fprintf(m_file, "************* file log begin ************\n");
	return 0;
}

int log::write_log(const char * msg, int len)
{
	if(!m_file)
	{
		printf_t("error: file is null\n");
		return -1;
	}
	
	time_t cur = time(NULL);
	time(&cur);
	struct tm tm_cur;
	memcpy(&tm_cur, localtime(&cur), sizeof(tm_cur));

	if(tm_cur.tm_mday != m_log_begin.tm_mday || m_cur_size >= m_max_size)
	{
		fflush(m_file);
		fclose(m_file);
		char newfilename[256] = {0};
		int ncopy = strlen(m_filename) - 4;
		strncpy(newfilename, m_filename, ncopy);
		
		if(tm_cur.tm_mday != m_log_begin.tm_mday){
			sprintf(newfilename + ncopy,"-%02d%02d%02d.log", 
			m_log_end.tm_hour, m_log_end.tm_min, m_log_end.tm_sec);
		}
		else{
			sprintf(newfilename + ncopy,"-%02d%02d%02d.log", 
			tm_cur.tm_hour,tm_cur.tm_min, tm_cur.tm_sec);
		}
		rename(m_filename, newfilename);
		
		return open_log();
	}
	
	m_log_end = tm_cur;
	m_cur_size += len;
	
	int ret = fwrite(msg, 1, len, m_file);
	if(ret < len)
	{
		printf_t("error: write %d/%d bytes error(%d)\n", ret, len, errno);
	}
	
	if(((m_times++) & 2) == 0)
	{
		fflush(m_file);
	}
	
	return 0;
}


int log::close_log()
{
	if(m_file)
	{
		fclose(m_file);
		m_file = NULL;
	}
	return 0;
}

/************************************************************/
tellog::tellog()
{
	m_brun = false;
}

tellog::~tellog()
{
	
}
void * tellog::tellog_task(void * param)
{
	tellog * t = (tellog*)param;
	t->run();
	return 0;
}

int tellog::run()
{
	while(m_brun)
	{

		void* data = NULL;
		
		if(m_ring_buf.pop(data) == -1)
		{
			continue;
		}
		
		if(!data)
		{
			printf_t("error: data is null\n");
			continue;
		}
		log_header * lh = (log_header*)data;

		m_telnet.echo(lh->msg, lh->color);
		
		m_log.write_log(lh->msg, lh->length);
		
		free(lh);
	}
	return 0;
}


int tellog::init()
{
	if(m_ring_buf.create(4000))
	{
		printf_t("error: ring buf init fail\n");
		return -1;
	}
	
	m_brun = true;
	pthread_t tid;
	if(create_thread(tid, tellog_task, "tellog", this) < 0)
	{
		printf_t("error: create thread fail %d\n", errno);
		return -1;
	}	
	return 0;
}

int tellog::init_log(const char* path, const char * name, int max_size)
{
	return m_log.init(path, name, max_size);
}

int tellog::init_telnet(const char * prompt, short port)
{
	return m_telnet.init(prompt, port);
}

int tellog::reg_cmd(const char* name, void* func)
{
	return m_telnet.reg_cmd(name, func);
}

int tellog::print(const char * msg, int color)
{
	int msg_len = strlen(msg);
	if(msg_len > max_log_len)
	{	
		printf_t("error: msg to len(%d)\n", msg_len);
		return -1;
	}
	
	int tal_len = msg_len + sizeof(log_header);
	log_header * lh = (log_header*)malloc(tal_len + 1);
	if(!lh)
	{
		printf_t("error: malloc log fail\n");
		return -1;
	}
	
	lh->color = color;
	lh->length = msg_len;
	lh->msg = (char*)lh + sizeof(log_header);
	memcpy(lh->msg, msg, msg_len);
	lh->msg[msg_len] = 0;
	
	if(m_ring_buf.push(lh) < 0)
	{
		free(lh);
		printf_t("error: push log queue fail\n");
		return -1;
	}
	return 0;
}