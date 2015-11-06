#include "watcher.h"
#include "rtsp.h"
#include "log.h"

int main(){

	debug_log("mysvr init %d\n", sizeof(int64));
	srand((unsigned)time( NULL ));
	watcher * s = watcher::instance();
	s->init();
	s->create_tcp_server(554);
	log::instance()->init("./log", "rtspsvr");
	
	for(int k = 0; k< 4; k++){
		rtsp * svr = new rtsp();
		s->reg_worker(svr);
	}
	return s->loop();
}