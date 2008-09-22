#include <stdio.h>
#include <string.h>

#include "timestamp.h"

struct event_name {
	const char* 	name;
	cmd_t	 	id;
};

#define EVENT(name) {#name, TS_ ## name ## _START}

static struct event_name event_table[] = 
{
	EVENT(SCHED),
	EVENT(SCHED2),
	EVENT(TICK),
	EVENT(RELEASE),
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
	EVENT(FIFO_DOWN),
	EVENT(PCP_UP),
	EVENT(PCP1_DOWN),
	EVENT(PCP2_DOWN),
	EVENT(DPCP_INVOKE),
	EVENT(DPCP_AGENT1),
	EVENT(DPCP_AGENT2),
	EVENT(MPCP_UP),
	EVENT(MPCP_DOWN),
	EVENT(SRPT)
};

int  str2event(const char* str, cmd_t *id)
{
	int i; 

	for (i = 0; i < sizeof(event_table) / sizeof(event_table[0]); i++)
		if (!strcmp(str, event_table[i].name)) {
			*id = event_table[i].id;
			return 1;
		}
	/* try to parse it as a number */
	return sscanf(str, "%u", id);
}

const char* event2str(cmd_t id)
{
	int i; 

	for (i = 0; i < sizeof(event_table) / sizeof(event_table[0]); i++)
		if (event_table[i].id == id)
			return event_table[i].name;
		
	return NULL;
}
