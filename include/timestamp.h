#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>

enum task_type_marker {
	TSK_BE,
	TSK_RT,
	TSK_UNKNOWN
};

/* Note: bitfields are highly non-portable (allocation is completely
 * implementation-defined).  Make sure you use the *same* compiler as the one
 * used for the kernel itself (this works with gcc in practice, but is not
 * *guaranteed* to work).  Patches to get rid of the bitfields (without
 * enlarging the structure) are highly welcome.
 */
struct timestamp {
	uint64_t		timestamp:48;
	uint64_t		pid:16;
	uint32_t		seq_no;
	uint8_t			cpu;
	uint8_t			event;
	uint8_t			task_type:2;
	uint8_t			irq_flag:1;
	uint8_t			irq_count:5;
};

typedef uint32_t cmd_t;

int  str2event(const char* str, cmd_t *id);
const char* event2str(cmd_t id);
const char* task_type2str(int task_type);

#define ENABLE_CMD	   0x0
#define DISABLE_CMD	   0x1
#define CALIBRATE_CMD	0x1410

#define TIMESTAMP(id) id

#define TS_SYSCALL_IN_START		TIMESTAMP(10)
#define TS_SYSCALL_IN_END		TIMESTAMP(11)

#define TS_SYSCALL_OUT_START		TIMESTAMP(20)
#define TS_SYSCALL_OUT_END		TIMESTAMP(21)

#define SUSPENSION_RANGE		30

#define TS_LOCK_START			TIMESTAMP(30)
#define TS_LOCK_END			TIMESTAMP(31)

#define TS_READ_LOCK_START		TIMESTAMP(32)
#define TS_READ_LOCK_END		TIMESTAMP(33)

#define TS_LOCK_SUSPEND			TIMESTAMP(38)
#define TS_LOCK_RESUME			TIMESTAMP(39)

#define TS_UNLOCK_START			TIMESTAMP(40)
#define TS_UNLOCK_END			TIMESTAMP(41)

#define TS_READ_UNLOCK_START		TIMESTAMP(42)
#define TS_READ_UNLOCK_END		TIMESTAMP(43)

#define PID_RECORDS_RANGE		99

#define TS_SCHED_START 			TIMESTAMP(100)
#define TS_SCHED_END			TIMESTAMP(101)
#define TS_SCHED2_START 		TIMESTAMP(102)
#define TS_SCHED2_END			TIMESTAMP(103)

#define TS_CXS_START			TIMESTAMP(104)
#define TS_CXS_END			TIMESTAMP(105)

#define TS_RELEASE_START		TIMESTAMP(106)
#define TS_RELEASE_END			TIMESTAMP(107)

#define TS_XCALL_START			TIMESTAMP(108)
#define TS_XCALL_END			TIMESTAMP(109)

#define TS_TICK_START  			TIMESTAMP(110)
#define TS_TICK_END    			TIMESTAMP(111)
#define TS_QUANTUM_BOUNDARY_START	TIMESTAMP(112)
#define TS_QUANTUM_BOUNDARY_END		TIMESTAMP(113)

#define TS_SCHED_TIMER_START	TIMESTAMP(114)
#define TS_SCHED_TIMER_END		TIMESTAMP(115)

#define TS_PLUGIN_SCHED_START		TIMESTAMP(120)
#define TS_PLUGIN_SCHED_END		TIMESTAMP(121)

#define TS_PLUGIN_TICK_START		TIMESTAMP(130)
#define TS_PLUGIN_TICK_END		TIMESTAMP(131)

#define TS_SEND_RESCHED_START		TIMESTAMP(190)
#define TS_SEND_RESCHED_END		TIMESTAMP(191)
#define TS_SEND_XCALL_START		TIMESTAMP(192)
#define TS_SEND_XCALL_END		TIMESTAMP(193)

#define SINGLE_RECORDS_RANGE		200

#define TS_RELEASE_LATENCY		TIMESTAMP(208)
#define TS_TIMER_LATENCY		TIMESTAMP(209)

#endif
