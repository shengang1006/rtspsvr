#include "conlist.h"

con_list::con_list()
{
	m_head = new con_item;
	m_head->prev = m_head;
	m_head->next = m_head;
	m_count = 0;
}

con_list::~con_list()
{
	clear();
	delete m_head;
}

void con_list::push_back(connection * n)
{	
	con_item * item = new con_item;
	n->set_inner_context(item);
	item->n = n;
	item->next = m_head;
	item->prev = m_head->prev;
	m_head->prev = item;
	item->prev->next = item;
	++m_count;
}

void con_list::push_front(connection * n)
{
	con_item * item = new con_item;
	n->set_inner_context(item);
	item->n = n;
	item->next = m_head->next;
	item->prev = m_head;
	m_head->next = item;
	item->next->prev = item;
	++m_count;
}


void con_list::move_to_back(connection * n)
{
	if(m_count)
	{
		con_item * item = (con_item *)n->get_inner_context();
		item->prev->next = item->next;
		item->next->prev = item->prev;
		
		item->next = m_head;
		item->prev = m_head->prev;
		m_head->prev = item;
		item->prev->next = item;
	
	}
}


void  con_list::move_to_front(connection * n)
{
	if(m_count)
	{
		con_item * item = (con_item *)n->get_inner_context();
		item->prev->next = item->next;
		item->next->prev = item->prev;
		
		item->next = m_head->next;
		item->prev = m_head;
		m_head->next = item;
		item->next->prev = item;
	}
}

void con_list::remove(connection * n)
{
	if(m_count)
	{
		con_item * item = (con_item *)n->get_inner_context();
		item->prev->next = item->next;
		item->next->prev = item->prev;
		--m_count;
		delete item;
	}
}

void con_list::clear()
{
	con_item * item = m_head->next;
	con_item * next = NULL;
	while(m_count)
	{
		next = item->next;
		delete item;
		item = next;
		--m_count;
	}
	m_head->prev = m_head;
	m_head->next = m_head;
}

	
connection* con_list::go_first()
{
	m_cur = m_head->next;
	return m_count ? m_cur->n : NULL;
}
	
connection* con_list::go_next()
{
	m_cur = m_cur->next;
	return m_cur == m_head ? NULL: m_cur->n;
}
