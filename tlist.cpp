#include "tlist.h"

void __init_head(list_node * head){
	head->prev = head; 
	head->next = head; 
	head->ptr  = NULL;
}

void __list_add(list_node *cur, list_node *prev, list_node *next){
    next->prev = cur;
	cur->next = next;
	cur->prev = prev;
    prev->next = cur;
}

void __list_del(list_node * prev, list_node * next){
	next->prev = prev;
	prev->next = next;
}

tlist::tlist(){
	m_head = new list_node;
	__init_head(m_head);
}

tlist::~tlist(){
	clear();
	delete m_head;
}

list_node* tlist::push_back(void * ptr){
	list_node * entry = new list_node;
	entry->ptr = ptr;
	__list_add(entry, m_head->prev, m_head);
	return entry;
}

list_node* tlist::push_front(void * ptr){
	list_node * entry = new list_node;
	entry->ptr = ptr;
	__list_add(entry, m_head, m_head->next);
	return entry;
}

void tlist::move_tail(list_node * entry){
	__list_del(entry->prev, entry->next);
    __list_add(entry, m_head->prev, m_head);
}

void tlist::move_head(list_node * entry){
	__list_del(entry->prev, entry->next);
    __list_add(entry, m_head, m_head->next);
}

void tlist::remove(list_node * entry){
	__list_del(entry->prev, entry->next);
	delete entry;
}

void tlist::clear(){
	list_node *pos = m_head->next;
	while(pos != m_head){
		list_node * next = pos->next;
		delete pos;
		pos = next;
	}
	__init_head(m_head);
}

list_node* tlist::begin(){
	return m_head->next;
}

list_node* tlist::end(){
	return m_head;
}

