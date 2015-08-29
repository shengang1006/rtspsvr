#pragma once
#include "utility.h"

struct evtime
{
	int id;         //定时id
	int appid;      //appid
	int interval;   //定时器间隔
	int index;      //索引
	int64 timeout;  //下次超时的超时时间
	void* ptr;      //定时回调数据
};


/*
 *	it is thread safe
 */
class timer
{
public:

	timer();

	virtual~timer();

	/*
	 *	init the time item
	 *  it is thread safe
	 */
	int init(int precision = 1000);

	/*
	 *	destroy the time item
	 *  it is thread safe
	 */
	int release();


	int add(int id, int interval, void* data, int appid = -1);

	/*
	 *	remove the time item
	 *  it is thread safe
	 */
	int remove(evtime* e);

	/*
	 *	pop the front item
	 *  it is thread safe
	 */
	int pop_timeout(evtime & ev);
	
	/*
	 *	get the first item timeout
	 *  it is thread safe
	 */
	int  timeout();

private:


	int min_heap_adjust_up(int holeindex, evtime * e);
	
	
	int min_heap_adjust_down(int holeindex, evtime * e);
	
	/*
	 *	比较两个evtime
	 */
	inline int compare(evtime * time1, evtime * time2);

private:
	evtime ** m_ppevtime;
	int m_max_heap_size;
	int m_cur_heap_size;
	auto_mutex m_mutex;
	int m_precision;
};

