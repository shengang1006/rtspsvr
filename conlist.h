#include "connection.h"

struct con_item
{
	con_item * next;
	con_item * prev;
	connection * n;
};

class con_list
{
public:
	con_list();
	
	~con_list();

	void push_back(connection* n);
	
	void push_front(connection* n);
	
	void move_to_back(connection* n);

	void move_to_front(connection* n);

	void remove(connection* n);
	
	void clear();
	
	connection* go_first();
	
	connection* go_next();
	
private:
	con_item * m_head;
	con_item * m_cur;
	int m_count;
};
