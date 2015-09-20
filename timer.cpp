
#include "timer.h"

timer::timer(){
	m_max_heap_size = 0;
	m_cur_heap_size = 0;
	m_ppevtime = NULL;
}

timer::~timer(){
	release();
}

/*
 *	init the time item
 */
int timer::init(){
	
	if (m_max_heap_size){
		error_log("timer already init %d\n", m_max_heap_size);
		return -1;
	}

	int init_size = 16;
	m_ppevtime = (evtime**)malloc(sizeof(evtime*) * init_size);

	if (!m_ppevtime){
		error_log("malloc memory for evtime\n");
		return -1;
	}

	memset(m_ppevtime, 0, sizeof(evtime*) * init_size);
	m_cur_heap_size = 0;
	m_max_heap_size = init_size;
	
	return 0;
}

/*
 *	destroy the time item
 */
int timer::release(){
	
	for(int k = 0; k< m_cur_heap_size; k++){
		delete m_ppevtime[k];
	}
	
	if(m_ppevtime){
		free(m_ppevtime);
		m_ppevtime = NULL;
	}

	m_cur_heap_size = 0;
	m_max_heap_size = 0;

	return 0;
}


int timer::add(int id, int interval, void* context){

	if (interval < 0){
		error_log("interval = %d < 0\n", interval);
		return -1;
	}

	if (m_max_heap_size == m_cur_heap_size){
		m_max_heap_size = m_max_heap_size ? m_max_heap_size * 2 : 8;
		m_ppevtime = (evtime**)realloc(m_ppevtime, sizeof(evtime*) * m_max_heap_size);
	}

	if (!m_ppevtime){
		error_log("realloc memory for evtime\n");
		return -1;
	}

	evtime * e = new evtime;
	e->id = id;
	e->interval = interval;
	e->ptr = context;
	e->timeout = get_tick_count() + interval;		//调整堆的大小

	min_heap_adjust_up(m_cur_heap_size++, e);

	return 0;
}

/*
 *	remove the time item
 */
int timer::remove(evtime* e){
	
	int res = -1;
	if (e->index != -1 && m_cur_heap_size){

		evtime * last = m_ppevtime[--m_cur_heap_size];
		int parent = (e->index - 1) >> 1;
		if (e->index > 0 && compare(m_ppevtime[parent], last) > 0){
			min_heap_adjust_up(e->index, last);
		}
		else{
			min_heap_adjust_down(e->index, last);
		}
		res =  0;
	}
	delete e;
	return res;
}

/*
 *	pop the front item
 */
int timer::pop_timeout(evtime & ev){

	if(!m_cur_heap_size){	
		return -1;
	}
	
	evtime * e = m_ppevtime[0];
	int64 timeout = e->timeout - get_tick_count();
	
	if(timeout <= 0){
		min_heap_adjust_down(0, m_ppevtime[--m_cur_heap_size]);
		memcpy(&ev, e, sizeof(ev));
		delete e;	
		return 0;
	}
		
	return -1;
}


/*
 * get the latency time
 */
int timer::latency_time(){

	if(!m_cur_heap_size){
		return -1;
	}

	evtime * e = m_ppevtime[0];
	
	int diff = (int)(e->timeout - get_tick_count());

	if (diff > e->interval){
		error_log("system time is forward adjusted\n");
	}

	if(diff < 0){
		return 0;
	}
	else{
		return diff;
	}
}

int timer::min_heap_adjust_up(int holeindex, evtime * e){

	int parent = (holeindex - 1) / 2;
	while(holeindex && compare(m_ppevtime[parent], e) > 0){
		(m_ppevtime[holeindex] = m_ppevtime[parent])->index = holeindex;
		holeindex = parent;
		parent = (holeindex - 1) / 2;
	}
	(m_ppevtime[holeindex] = e)->index = holeindex;

	return holeindex;
}

int timer::min_heap_adjust_down(int holeindex, evtime * e){

	int minchild = (holeindex + 1) << 1;
	while(minchild <= m_cur_heap_size){
		minchild -= (minchild == m_cur_heap_size) || (compare(m_ppevtime[minchild], m_ppevtime[minchild-1]) > 0);
		if (compare(e, m_ppevtime[minchild]) <= 0){
			break;
		}
		(m_ppevtime[holeindex] = m_ppevtime[minchild])->index = holeindex;
		holeindex = minchild;
		minchild = (holeindex + 1) << 1;
	}
	(m_ppevtime[holeindex] = e)->index = holeindex;

	return holeindex;
}

/*
 *	比较两个evtime
 */
int timer::compare(evtime * time1, evtime * time2){
	int64 diff = time1->timeout - time2->timeout;
	if(diff < 0){
		return -1;
	}
	else if(diff > 0){
		return 1;
	}
	else{
		return 0;
	}
}

