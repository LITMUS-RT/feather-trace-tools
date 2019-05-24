/*    ft2csv -- Convert start and end events from a Feather-Trace
 *              event stream into a CSV file.
 *    Copyright (C) 2007, 2008  B. Brandenburg.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "mapping.h"

#include "timestamp.h"

#define AUTO_SELECT -1

static int want_interleaved    = 1;
static int want_best_effort    = 0;
static int want_interrupted    = 0;
static int max_interleaved_skipped = 0;
static int find_by_pid         = AUTO_SELECT;

/* discard samples from a specific CPU */
static int avoid_cpu = -1;
/* only use samples from a specific CPU */
static int only_cpu  = -1;

static unsigned int complete   = 0;
static unsigned int incomplete = 0;
static unsigned int interrupted = 0;
static unsigned int skipped    = 0;
static unsigned int non_rt     = 0;
static unsigned int interleaved = 0;
static unsigned int avoided    = 0;

static struct timestamp* next(struct timestamp* first, struct timestamp* end,
			      int cpu)
{
	struct timestamp* pos;
	uint32_t last_seqno = 0, next_seqno = 0;


	last_seqno = first->seq_no;
	for (pos = first + 1; pos < end;  pos++) {
		/* check for for holes in the sequence number */
		next_seqno = last_seqno + 1;
		if (next_seqno != pos->seq_no) {
			/* stumbled across a hole */
			return NULL;
		}
		last_seqno = pos->seq_no;

		if (pos->cpu == cpu)
			return pos;
	}
	return NULL;
}

static struct timestamp* next_id(struct timestamp* start, struct timestamp* end,
				 int cpu, unsigned long id,
				 unsigned long stop_id,
				 int *interrupt_flag)
{
	struct timestamp* pos = start;
	int restarts = 0;

	while ((pos = next(pos, end, cpu))) {
		if (pos->event == id)
			break;
		else if (pos->event == stop_id)
			return NULL;

		restarts++;
		if (!want_interleaved)
			return NULL;
		if (pos->irq_flag && !want_interrupted) {
			*interrupt_flag = 1;
			return NULL;
		}
	}
	if (pos)
		interleaved += restarts;

	if (pos && pos->irq_flag) {
		interrupted++;
		if (!want_interrupted) {
			*interrupt_flag = 1;
			return NULL;
		}
	}

	return pos;
}

static struct timestamp* find_second_ts(struct timestamp* start,
					struct timestamp* end,
					int *interrupt_flag)
{
	/* convention: the end->event is start->event + 1 */
	return next_id(start, end, start->cpu, start->event + 1,
		       start->event, interrupt_flag);
}

static struct timestamp* next_pid(struct timestamp* first, struct timestamp* end,
				  unsigned long id1, unsigned long id2,
				  int interrupts_significant, int *interrupted_flag)
{
	struct timestamp* pos;
	uint32_t last_seqno = 0, next_seqno = 0;
	int event_count = 0;


	last_seqno = first->seq_no;
	for (pos = first + 1; pos < end;  pos++) {
		event_count = event_count + 1;
		/* check for for holes in the sequence number */
		next_seqno = last_seqno + 1;
		if (next_seqno != pos->seq_no) {
			/* stumbled across a hole */
			return NULL;
		}
		last_seqno = pos->seq_no;

		if (interrupts_significant
		    && pos->cpu == first->cpu
		    && pos->irq_flag) {
			/* did an interrupt get in the way? */
			interrupted++;
			if (!want_interrupted) {
				*interrupted_flag = 1;
				return NULL;
			}
		}

		/* only care about this PID */
		if (pos->pid == first->pid) {
			/* is  it the right one? */
			if (pos->event == id1 || pos->event == id2)
				return pos;
			else
				if (event_count > max_interleaved_skipped)
					/* Don't allow unexpected IDs interleaved.
					 * Tasks are sequential, there shouldn't be
					 * anything else. */
					return NULL;
		}
	}
	return NULL;
}

static struct timestamp* skip_over_suspension(struct timestamp *pos,
					      struct timestamp *end,
					      uint64_t *last_time)
{
	/* Find matching resume. */
	pos = next_pid(pos, end,
		       TS_LOCK_RESUME, TS_SCHED_START,
		       0, NULL);

	if (!pos || pos->timestamp < *last_time)
		/* broken stream */
		return NULL;

	*last_time = pos->timestamp;

	if (pos->event == TS_SCHED_START) {
		/* Was scheduled out, find TS_SCHED_END. */
		pos = next_pid(pos, end, TS_SCHED_END, 0, 0, NULL);
		if (!pos || pos->timestamp < *last_time)
			return NULL;

		/* next find TS_LOCK_RESUME */
		pos = next_pid(pos, end, TS_LOCK_RESUME, 0, 0, NULL);
	}


	return pos;
}

static struct timestamp* accumulate_exec_time(
	struct timestamp* start, struct timestamp* end,
	unsigned long stop_id, uint64_t *sum,
	int *interrupted_flag)
{
	struct timestamp *pos = start;
	uint64_t exec_start;
	uint64_t last_time = start->timestamp;

	*sum = 0;

	while (1) {
		exec_start = pos->timestamp;

		/* Find a suspension or the proper end. */
		pos = next_pid(pos, end,
			       TS_LOCK_SUSPEND, stop_id,
			       1, interrupted_flag);

		if (!pos || pos->timestamp < last_time)
			/* broken stream */
			return NULL;

		/* account for exec until pos */
		*sum += pos->timestamp - exec_start;

		if (pos->event == stop_id)
			/* no suspension or preemption */
			return pos;
		else {
			last_time = pos->timestamp;

			/* handle self-suspension */
			pos = skip_over_suspension(pos, end, &last_time);

			/* Must be a resume => start over.  If a resume sample is
			 * affected by interrupts we don't care since it does not
			 * contribute to the reported execution cost.
			 */

			if (!pos || pos->timestamp < last_time)
				/* broken stream */
				return NULL;

			last_time = pos->timestamp;
		}
	}
}

typedef void (*pair_fmt_t)(struct timestamp* first, struct timestamp* second, uint64_t exec_time);

static void print_pair_csv(struct timestamp* first, struct timestamp* second, uint64_t exec_time)
{
	printf("%llu, %llu, %llu\n",
	       (unsigned long long) first->timestamp,
	       (unsigned long long) second->timestamp,
	       (unsigned long long) exec_time);
}

static void print_pair_bin(struct timestamp* first, struct timestamp* second, uint64_t exec_time)
{
	float delta =  exec_time;
	fwrite(&delta, sizeof(delta), 1, stdout);
}

pair_fmt_t format_pair = print_pair_csv;

static void find_event_by_pid(struct timestamp* first, struct timestamp* end)
{
	struct timestamp *second;
	uint64_t exec_time = 0;
	int interrupted = 0;

	/* special case: take suspensions into account */
	if (first->event >= SUSPENSION_RANGE && max_interleaved_skipped == 0) {
		second = accumulate_exec_time(first, end,
					      first->event + 1, &exec_time,
					      &interrupted);
	} else {
		second = next_pid(first, end,
				  first->event + 1, 0,
				  1, &interrupted);
		if (second && second->timestamp > first->timestamp)
			exec_time = second->timestamp - first->timestamp;
		else
			second = NULL;
	}
	if (second) {
		format_pair(first, second, exec_time);
		complete++;
	} else if (!interrupted)
		incomplete++;
}

static void find_event_by_eid(struct timestamp *first, struct timestamp* end)
{
	struct timestamp *second;
	uint64_t exec_time;
	int interrupted = 0;

	second = find_second_ts(first, end, &interrupted);
	if (second && second->timestamp > first->timestamp) {
		exec_time = second->timestamp - first->timestamp;
		if (first->task_type != TSK_RT &&
			 second->task_type != TSK_RT && !want_best_effort)
			non_rt++;
		else {
			format_pair(first, second, exec_time);
			complete++;
		}
	} else if (!interrupted)
		incomplete++;
}

static void show_csv(struct timestamp* first, struct timestamp *end)
{


	if (first->cpu == avoid_cpu ||
	    (only_cpu != -1 && first->cpu != only_cpu)) {
		avoided++;
		return;
	}

	if (find_by_pid)
		find_event_by_pid(first, end);
	else
		find_event_by_eid(first, end);
}

typedef void (*single_fmt_t)(struct timestamp* ts);

static void print_single_csv(struct timestamp* ts)
{
	printf("0, 0, %llu\n",
	       (unsigned long long) (ts->timestamp));
}

static void print_single_bin(struct timestamp* ts)
{
	float delta = ts->timestamp;

	fwrite(&delta, sizeof(delta), 1, stdout);
}

single_fmt_t single_fmt = print_single_csv;

static void show_single(struct timestamp* ts)
{
	if (ts->cpu == avoid_cpu ||
	    (only_cpu != -1 && ts->cpu != only_cpu)) {
		avoided++;
	} else if (ts->task_type == TSK_RT) {
		single_fmt(ts);
		complete++;
	} else
		non_rt++;
}

static void show_id(struct timestamp* start, struct timestamp* end,
		    unsigned long id)
{
	while (start !=end && start->event != id + 1) {
		skipped++;
		start++;
	}

	for (; start != end; start++)
		if (start->event == id)
			show_csv(start, end);
}

static void show_single_records(struct timestamp* start, struct timestamp* end,
				unsigned long id)
{
	for (; start != end; start++)
		if (start->event == id)
			show_single(start);
}

static void list_ids(struct timestamp* start, struct timestamp* end)
{
	unsigned int already_seen[256] = {0};
	const char *name;

	for (; start != end; start++)
		if (!already_seen[start->event])
		{
			already_seen[start->event] = 1;
			name = event2str(start->event);
			if (name)
				printf("%s\n", name);
			else
				printf("%d\n", start->event);
		}
}


#define USAGE								\
	"Usage: ft2csv [-r] [-i] [-s NUM] [-b] [-a CPU] [-o CPU] <event_name>  <logfile> \n" \
	"   -i: ignore interleaved  -- ignore samples if start "	\
	"and end are non-consecutive\n"					\
	"   -s: max_interleaved_skipped -- maximum number of skipped interleaved samples. "	\
	"must be used in conjunction with [-i] option \n" \
	"   -b: best effort         -- don't skip non-rt time stamps \n" \
	"   -r: raw binary format   -- don't produce .csv output \n"	\
	"   -a: avoid CPU           -- skip samples from a specific CPU\n" \
	"   -o: only CPU            -- skip all samples from other CPUs\n" \
	"   -x: allow interrupts    -- don't skip samples with IRQ-happned flag\n" \
	"   -l: list events         -- list the events present in the file\n" \
	"   -h: help                -- show this help message\n" \
	""

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "%s", USAGE);
	exit(1);
}

#define OPTS "ibrs:a:o:pexhl"

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp *ts, *end;
	cmd_t id;
	int opt;
	char event_name[80];
	int list_events = 0;

	while ((opt = getopt(argc, argv, OPTS)) != -1) {
		switch (opt) {
		case 'i':
			want_interleaved = 0;
			fprintf(stderr, "Discarging interleaved samples.\n");
			break;
		case 's':
			max_interleaved_skipped = atoi(optarg);
			fprintf(stderr, "skipping up to %d interleaved samples.\n",
					max_interleaved_skipped);
			break;
		case 'x':
			want_interrupted = 1;
			fprintf(stderr, "Not filtering disturbed-by-interrupt samples.\n");
			break;
		case 'b':
			fprintf(stderr,"Not filtering samples from best-effort"
				" tasks.\n");
			want_best_effort = 1;
			break;
		case 'r':
			fprintf(stderr, "Generating binary, NumPy-compatible output.\n");
			single_fmt  = print_single_bin;
			format_pair = print_pair_bin;
			break;
		case 'a':
			avoid_cpu = atoi(optarg);
			fprintf(stderr, "Disarding all samples from CPU %d.\n",
				avoid_cpu);
			break;
		case 'o':
			only_cpu = atoi(optarg);
			fprintf(stderr, "Using only samples from CPU %d.\n",
				only_cpu);
			break;
		case 'p':
			find_by_pid = 1;
			fprintf(stderr, "Matching timestamp pairs based on PID.\n");
			break;
		case 'e':
			find_by_pid = 0;
			fprintf(stderr, "Matching timestamp pairs based on event ID.\n");
			break;
		case 'l':
			list_events = 1;
			break;
		case 'h':
			errno = 0;
			die("");
			break;
		default:
			die("Unknown option.");
			break;
		}
	}


	if (list_events) {
		/* no event ID specified */
		if (argc - optind != 1)
			die("arguments missing");
		if (map_file(argv[optind], &mapped, &size))
			die("could not map file");
	} else {
		if (argc - optind != 2)
			die("arguments missing");
		if (map_file(argv[optind + 1], &mapped, &size))
			die("could not map file");
	}

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

	if (list_events) {
		list_ids(ts, end);
		return 0;
	}

	if (!str2event(argv[optind], &id)) {
		/* see if it is a short name */
		snprintf(event_name, sizeof(event_name), "%s_START",
			  argv[optind]);
		if (!str2event(event_name, &id))
			die("Unknown event!");
	}

	if (find_by_pid == AUTO_SELECT)
		find_by_pid = id <= PID_RECORDS_RANGE;

	if (id >= SINGLE_RECORDS_RANGE)
		show_single_records(ts, end, id);
	else
		show_id(ts, end, id);

	if (count == skipped)
		fprintf(stderr, "Event %s not present.\n",
			argv[optind]);
	else
		fprintf(stderr,
			"Total       : %10d\n"
			"Skipped     : %10d\n"
			"Avoided     : %10d\n"
			"Complete    : %10d\n"
			"Incomplete  : %10d\n"
			"Non RT      : %10d\n"
			"Interleaved : %10d\n"
			"Interrupted : %10d\n",
			(int) count,
			skipped, avoided, complete,
			incomplete, non_rt,
			interleaved, interrupted);

	return 0;
}
