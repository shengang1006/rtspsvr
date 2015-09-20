#pragma once
#include "utility.h"

struct list_node{
	list_node *next;
	list_node *prev;
	void *ptr;
};

class tlist{
	
public:
	tlist();
	virtual ~tlist();	
	list_node * push_back(void * ptr);
	list_node * push_front(void * ptr);	
	void move_tail(list_node * entry);
	void move_head(list_node * entry);
	void remove(list_node * entry);
	void clear();
	list_node* begin();
	list_node* end();
private:
	list_node *m_head;
};

