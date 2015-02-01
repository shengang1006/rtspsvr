#pragma once
#include "utility.h"

typedef int (*bind_func)(char*,char*,char*,char*,char*,char*,char*,char*,char*,char*);
#define max_cmd_len  128
#define max_cmd_history 64
#define max_cmd_reg  64
#define max_log_len  1024 * 8

#define telcmd_will    251
#define telcmd_wont    252
#define telcmd_do      253
#define telcmd_dont    254
#define telcmd_iac     255

#define telopt_echo     1
#define telopt_sga      3
#define telopt_lflow    33
#define telopt_naws     34


#define backspace_char  8
#define blank_char      32
#define return_char     13
#define tab_char        9
#define del_char       127
#define ctrl_s          19
#define ctrl_r          18
#define ctrl_c          3

#define arrow_first     27
#define arrow_second    91
#define arrow_up        65
#define arrow_down      66
#define arrow_left      68
#define arrow_right     67

enum 
{
	color_black = 0,
	color_red,
	color_light_red,
	color_green,
	color_light_green,
	color_blue,
	color_light_blue,
	color_dark_gray,
	color_cyan,
	color_light_cyan,
	color_purple,
	color_light_purple,
	color_brown,
	color_yellow,
	color_light_gray,
	color_white,
	color_none,
};

struct cmd_history
{
	char vec_cmd[max_cmd_history][max_cmd_len];
	int tail;
	int pos;	
}; 

struct cmd_reg
{
	char cmd[max_cmd_len];
	bind_func fn;
};

struct log_header
{
	int color;
	int length;
	char * msg;
};

enum{log_info = 0, log_debug, log_warn, log_error, log_none};

class telnet
{
public:
	telnet();
	virtual ~telnet();
	int init( const char * prompt, short port);
	int echo(const char * msg, int color);
	int reg_cmd(const char* name, void* func);

protected:
    int send_nvt_cmd(char cmd, char opt);
	int lg_send(const char * msg, int len);
	int lg_run();
	int lg_accept();
	int lg_recv();
	int lg_replace(char * last, char * cur);
	int lg_close();
	void set_cmd(char * pcmd);
	char* next_cmd(int mode);
	int run_cmd(char * pcmd);
	static void * telnet_task(void * param);
	bool isprint_char(char ch);

private:
	
	int m_epfd;
	int m_listenfd;
	int m_telport;
	int m_clientfd;
	bool m_brun;
	char m_prompt[64];
	char m_curcmd[max_cmd_len];
	cmd_history  m_cmd_history;
	cmd_reg m_cmd_table[max_cmd_reg];
	int m_regs;
	bool m_stop_echo;
};

class log
{
public:
	log();
	virtual ~log();
	int init(const char* path, const char * name, int max_size); 
	int open_log();
	int write_log(const char * msg, int len);
	int close_log();
private:
	FILE * m_file;
	char m_pathname[256];
	char m_filename[256];
	int m_max_size;
	int m_times;
	struct tm m_logbegin;
	struct tm m_logend;
};

class tellog
{
public:
	tellog();
	virtual ~tellog();
	int init();
	int init_log(const char* path, const char * name, int max_size); 
	int init_telnet(const char * prompt, short port); 
	int print(const char * msg, int color);
	int reg_cmd(const char* name, void* func);
	int run();
	static void * tellog_task(void * param);
private:
	ring_buffer m_ring_buf;
	bool m_brun;
	telnet m_telnet;
	log m_log;
};