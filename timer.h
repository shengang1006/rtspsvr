#pragma once
#include "utility.h"

struct evtime{
	
	int id;         //定时id
	int interval;   //定时器间隔
	int index;      //索引
	int64 timeout;  //下次超时的超时时间
	void* ptr;      //定时回调数据
};


/*
 *	it is thread safe
 */
class timer{
	
public:
	timer();

	virtual~timer();

	/*
	 *	init the time item
	 */
	int init();

	/*
	 *	destroy the time item
	 */
	int release();
	
	/*
	 *	add the time item
	 */
	int add(int id, int interval, void* data);

	/*
	 *	pop the front item
	 */
	int pop_timeout(evtime & ev);
	
	/*
	 *	get the latency time
	 */
	int latency_time();
private:

	/*
	 *	remove the time item
	 */
	int remove(evtime* e);
	

	int min_heap_adjust_up(int holeindex, evtime * e);
	
	
	int min_heap_adjust_down(int holeindex, evtime * e);
	
	/*
	 *	compare evtime, return -1,0,1
	 */
	inline int compare(evtime * time1, evtime * time2);

private:
	evtime ** m_ppevtime;
	int m_max_heap_size;
	int m_cur_heap_size;
};

