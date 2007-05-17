#include <string.h>

#include "timestamp.h"

struct event_name {
	const char* 	name;
	unsigned long 	id;
};

#define EVENT(name) {#name, TS_ ## name ## _START}

static struct event_name event_table[] = 
{
	EVENT(SCHED),
	EVENT(TICK),
	EVENT(PLUGIN_SCHED),
	EVENT(PLUGIN_TICK),
	EVENT(CXS),
	EVENT(ENTER_NP),
	EVENT(EXIT_NP),
	EVENT(SRP_UP),
	EVENT(SRP_DOWN),
	EVENT(PI_UP),
	EVENT(PI_DOWN),
	EVENT(FIFO_UP),
	EVENT(FIFO_DOWN)
};

int  str2event(const char* str, unsigned long *id)
{
	int i; 

	for (i = 0; i < sizeof(event_table) / sizeof(event_table[0]); i++)
		if (!strcmp(str, event_table[i].name)) {
			*id = event_table[i].id;
			return 1;
		}
	return 0;
}

const char* event2str(unsigned long id)
{
	int i; 

	for (i = 0; i < sizeof(event_table) / sizeof(event_table[0]); i++)
		if (event_table[i].id == id)
			return event_table[i].name;
		
	return NULL;
}
