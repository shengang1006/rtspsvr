#pragma once
#include "utility.h"

#define max_log_len  1024 * 8

class log{

private:
	log();
	log(const log &);  
    log & operator = (const log &);
	
public:
	static log * instance();
	virtual~ log();
	int init(const char* path, const char * name, int max_size = 8<<20); 
	int write_log(const char * msg);
	int close_log();

protected:
	int run();
	int open_log();
	int write_log(const char * msg, int len);
	static void * log_task(void * param);

private:
	FILE * m_file;
	char m_pathname[256];
	char m_filename[256];
	int m_max_size;
	int m_times;
	struct tm m_logbegin;
	struct tm m_logend;

	ring_buffer m_ring_buf;
	auto_mutex m_task_mutex;

	bool m_brun;
};

