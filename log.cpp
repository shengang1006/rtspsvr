#include "log.h"

struct log_header{
	int length;
	char * msg;
};


log::log(){
	m_file = NULL;
	m_times = 0;
	memset(m_pathname, 0, sizeof(m_pathname));
	memset(m_filename, 0, sizeof(m_filename));
}

log::~log(){
	close_log();
}


log * log::instance(){
	static log inst;
	return &inst;
}

void * log::log_task(void * param){

	log * t = (log*)param;
	t->run();
	return 0;
}


int log::run(){

	auto_lock lock(m_task_mutex);

	while(true){
		
		void* data = NULL;
		
		if(m_ring_buf.pop(data) == -1){
			continue;
		}

		if(!m_brun){
			free(data);
			break;
		}

		log_header * lh = (log_header*)data;
		write_log(lh->msg, lh->length);
		
		free(lh);
	}
	return 0;
}


int log::init(const char* path, const char * name, int max_size /*= 8<<20*/){

	if(create_directory(path) < 0){
		return -1;
	}

	m_max_size = max_size <= (2<<10) ? (2<<10): max_size;
	strcpy(m_pathname, path);
	if(m_pathname[strlen(m_pathname) -1] != '/'){
		strcat(m_pathname, "/");
	}
	strcat(m_pathname, name);

	
	if(m_ring_buf.create(4000)){
		printf_t("ring_buf create fail\n");
		return -1;
	}

	m_brun = true;
	pthread_t tid;
	if(create_thread(tid, log_task, "log", this) < 0){
		printf_t("create thread fail %d\n", errno);
		return -1;
	}	

	return open_log();
}

int log::open_log(){
	
	time_t cur = time(NULL);
	localtime_r(&cur, &m_logbegin);

	snprintf(m_filename, sizeof(m_filename),"%s-%d%02d%02d.log", 
			m_pathname,
			m_logbegin.tm_year+1900, 
			m_logbegin.tm_mon+1,
			m_logbegin.tm_mday);
		
	m_file = fopen(m_filename, "a+");
	
	if(!m_file){
		return -1;
	}
	
	fseek(m_file, 0L, SEEK_END);
	m_times = 0;
	fprintf(m_file, "************* file log begin ************\n");
	return 0;
}

int log::write_log(const char * msg){

	int msg_len = strlen(msg);
	if(msg_len > max_log_len){	
		printf_t("msg to len(%d)\n", msg_len);
		return -1;
	}
	
	int total = msg_len + sizeof(log_header);
	log_header * lh = (log_header*)malloc(total);
	if(!lh){
		printf_t("malloc log fail %d\n", errno);
		return -1;
	}
	
	lh->length = msg_len;
	lh->msg = (char*)lh + sizeof(log_header);
	memcpy(lh->msg, msg, msg_len);

	if(m_ring_buf.push(lh) < 0){
		free(lh);
		return -1;
	}
	return 0;
}

int log::write_log(const char * msg, int len){

	if(!m_file){
		printf_t("file is not open\n");
		return -1;
	}
	
	time_t cur = time(NULL);
	struct tm tm_cur ;
	localtime_r(&cur, &tm_cur);
	
	int ret = fwrite(msg, 1, len, m_file);
	if(ret < len){
		printf_t("write %d/%d bytes error(%d)\n", ret, len, errno);
	}
	
	if(((m_times++) & 2) == 0){
		fflush(m_file);
	}
	
	int cur_size = (int)ftell(m_file);
	
	if(cur_size < m_max_size && m_logbegin.tm_mday == tm_cur.tm_mday){
		return 0;
	}
	
	fprintf(m_file, "file log %d-%d day end write %d/%d bytes\n", 
	m_logbegin.tm_mday , tm_cur.tm_mday, cur_size, m_max_size);
	
	fclose(m_file);
	
	if(m_logbegin.tm_mday != tm_cur.tm_mday){
		tm_cur.tm_hour = 24;
		tm_cur.tm_min = 0; 
		tm_cur.tm_sec = 0;
	}

	char newfilename[512] = {0};
	snprintf(newfilename ,sizeof(newfilename),
		"%s-%d%02d%02d-%02d%02d%02d.log", 
		m_pathname,
		m_logbegin.tm_year+1900, 
		m_logbegin.tm_mon+1,
		m_logbegin.tm_mday,
		tm_cur.tm_hour,
		tm_cur.tm_min, 
		tm_cur.tm_sec);

	rename(m_filename, newfilename);
	return open_log();
}


int log::close_log(){

	write_log("exit");
	m_brun = false;

	{auto_lock lock(m_task_mutex);}

	if(m_file){
		fclose(m_file);
		m_file = NULL;
	}

	void* data = NULL;
	while(m_ring_buf.pop(data, 100) != -1){
		free(data);
	}

	return 0;
}

