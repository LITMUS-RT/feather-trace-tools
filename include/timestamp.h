#ifndef TIMESTAMP_H
#define TIMESTAMP_H

struct timestamp {
	unsigned long		event;
	unsigned long long	timestamp;
	unsigned int		seq_no;
	int			cpu;
};

int  str2event(const char* str, unsigned long *id);
const char* event2str(unsigned long id);

#define ENABLE_CMD 0L

#define TIMESTAMP(id) id ## L

#define TS_SCHED_START 			TIMESTAMP(100)
#define TS_SCHED_END			TIMESTAMP(101)
#define TS_CXS_START			TIMESTAMP(102)
#define TS_CXS_END			TIMESTAMP(103)

#define TS_TICK_START  			TIMESTAMP(110)
#define TS_TICK_END    			TIMESTAMP(111)

#define TS_PLUGIN_SCHED_START		TIMESTAMP(120)
#define TS_PLUGIN_SCHED_END		TIMESTAMP(121)

#define TS_PLUGIN_TICK_START		TIMESTAMP(130)
#define TS_PLUGIN_TICK_END		TIMESTAMP(131)





#endif
