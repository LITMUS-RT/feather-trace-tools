#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>

enum task_type_marker {
	TSK_BE,
	TSK_RT,
	TSK_UNKNOWN
};

struct timestamp {
	uint64_t		timestamp;
	uint32_t		seq_no;
	uint8_t			cpu;
	uint8_t			event;
	uint8_t			task_type;
};

typedef uint32_t cmd_t;

int  str2event(const char* str, cmd_t *id);
const char* event2str(cmd_t id);
const char* task_type2str(int task_type);

#define ENABLE_CMD  0L
#define DISABLE_CMD 1L

#define TIMESTAMP(id) id

#define TS_SYSCALL_IN_START		TIMESTAMP(10)
#define TS_SYSCALL_IN_END		TIMESTAMP(11)

#define TS_SYSCALL_OUT_START		TIMESTAMP(20)
#define TS_SYSCALL_OUT_END		TIMESTAMP(21)


#define TS_SCHED_START 			TIMESTAMP(100)
#define TS_SCHED_END			TIMESTAMP(101)
#define TS_SCHED2_START 		TIMESTAMP(102)
#define TS_SCHED2_END			TIMESTAMP(103)

#define TS_CXS_START			TIMESTAMP(104)
#define TS_CXS_END			TIMESTAMP(105)

#define TS_RELEASE_START		TIMESTAMP(106)
#define TS_RELEASE_END			TIMESTAMP(107)

#define TS_TICK_START  			TIMESTAMP(110)
#define TS_TICK_END    			TIMESTAMP(111)

#define TS_PLUGIN_SCHED_START		TIMESTAMP(120)
#define TS_PLUGIN_SCHED_END		TIMESTAMP(121)

#define TS_PLUGIN_TICK_START		TIMESTAMP(130)
#define TS_PLUGIN_TICK_END		TIMESTAMP(131)

#define TS_LOCK_START			TIMESTAMP(170)
#define TS_LOCK_SUSPEND			TIMESTAMP(171)
#define TS_LOCK_RESUME			TIMESTAMP(172)
#define TS_LOCK_END			TIMESTAMP(173)

#define TS_UNLOCK_START			TIMESTAMP(180)
#define TS_UNLOCK_END			TIMESTAMP(181)

#define TS_SEND_RESCHED_START		TIMESTAMP(190)
#define TS_SEND_RESCHED_END		TIMESTAMP(191)

#define SINGLE_RECORDS_RANGE		200

#define TS_RELEASE_LATENCY		TIMESTAMP(208)

#endif
