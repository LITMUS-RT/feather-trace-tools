#ifndef __SCHED_TRACE_H_
#define __SCHED_TRACE_H_

#include <stdint.h>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;

/* A couple of notes about the format:
 *
 * - make sure it is in sync with the kernel
 * - all times in nanoseconds
 * - endianess issues are the problem of the app
 */

struct st_trace_header {
	u8	type;		/* Of what type is this record?  */
	u8	cpu;		/* On which CPU was it recorded? */
	u16	pid;		/* PID of the task.              */
	u32	job;		/* The job sequence number.      */
};

#define ST_NAME_LEN 16
struct st_name_data {
	char	cmd[ST_NAME_LEN];/* The name of the executable of this process. */
};

struct st_param_data {		/* regular params */
	u32	wcet;
	u32	period;
	u32	phase;
	u8	partition;
	u8	__unused[3];
};

struct st_release_data {	/* A job is was/is going to be released. */
	u64	release;	/* What's the release time?              */
	u64	deadline;	/* By when must it finish?		 */
};

struct st_assigned_data {	/* A job was asigned to a CPU. 		 */
	u64	when;
	u8	target;		/* Where should it execute?	         */
	u8	__unused[3];
};

struct st_switch_to_data {	/* A process was switched to on a given CPU.   */
	u64	when;		/* When did this occur?                        */
	u32	exec_time;	/* Time the current job has executed.          */

};

struct st_switch_away_data {	/* A process was switched away from on a given CPU. */
	u64	when;
	u64	exec_time;
};

struct st_completion_data {	/* A job completed. */
	u64	when;
	u64	forced:1; 	/* Set to 1 if job overran and kernel advanced to the
				 * next job automatically; set to 0 otherwise.
				 */
	u64	exec_time:63; /* Actual execution time of job. */
};

struct st_block_data {		/* A task blocks. */
	u64	when;
	u64	__unused;
};

struct st_resume_data {		/* A task resumes. */
	u64	when;
	u64	__unused;
};

struct st_np_enter_data {	/* A task becomes non-preemptable.  */
	u64	when;
	u64	__unused;
};

struct st_np_exit_data {       	/* A task becomes preemptable again. */
	u64	when;
	u64	__unused;
};

struct st_action_data {		/* Catch-all for misc. events. */
	u64	when;
	u8	action;
	u8	__unused[7];
};

struct st_sys_release_data {
	u64	when;
	u64	release;
};

#define DATA(x) struct st_ ## x ## _data x;

typedef enum {
        ST_NAME = 1,		/* Start at one, so that we can spot
				 * uninitialized records. */
	ST_PARAM,
	ST_RELEASE,
	ST_ASSIGNED,
	ST_SWITCH_TO,
	ST_SWITCH_AWAY,
	ST_COMPLETION,
	ST_BLOCK,
	ST_RESUME,
	ST_ACTION,
	ST_SYS_RELEASE,
	ST_NP_ENTER,
	ST_NP_EXIT,

	ST_INVALID              /* Dummy ID to catch too-large IDs. */
} st_event_record_type_t;

struct st_event_record {
	struct st_trace_header hdr;
	union {
		u64 raw[2];

		DATA(name);
		DATA(param);
		DATA(release);
		DATA(assigned);
		DATA(switch_to);
		DATA(switch_away);
		DATA(completion);
		DATA(block);
		DATA(resume);
		DATA(action);
		DATA(sys_release);
		DATA(np_enter);
		DATA(np_exit);
	} data;
};

#undef DATA

const char* event2name(unsigned int id);
u64 event_time(struct st_event_record* rec);

void print_event(struct st_event_record *rec);
void print_all(struct st_event_record *rec, unsigned int count);

#endif
