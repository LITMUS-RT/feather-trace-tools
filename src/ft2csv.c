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
static int find_by_pid         = AUTO_SELECT;

/* discard samples from a specific CPU */
static int avoid_cpu = -1;
/* only use samples from a specific CPU */
static int only_cpu  = -1;

static unsigned int complete   = 0;
static unsigned int incomplete = 0;
static unsigned int skipped    = 0;
static unsigned int non_rt     = 0;
static unsigned int interleaved = 0;
static unsigned int avoided    = 0;

#define CYCLES_PER_US 2128

static struct timestamp* next(struct timestamp* start, struct timestamp* end,
			      int cpu)
{
	struct timestamp* pos;
	unsigned int last_seqno = 0;

	for (pos = start; pos != end;  pos++) {
		/* check for for holes in the sequence number */
		if (last_seqno && last_seqno + 1 != pos->seq_no) {
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
				 unsigned long stop_id)
{
	struct timestamp* pos = start;
	int restarts = 0;

	while ((pos = next(pos, end, cpu))) {
		if (pos->event == id)
			break;
		else if (pos->event == stop_id)
			return NULL;
		pos++;
		restarts++;
		if (!want_interleaved)
			return NULL;
	}
	if (pos)
		interleaved += restarts;
	return pos;
}

static struct timestamp* find_second_ts(struct timestamp* start,
					struct timestamp* end)
{
	/* convention: the end->event is start->event + 1 */
	return next_id(start + 1, end, start->cpu, start->event + 1,
		       start->event);
}

static struct timestamp* next_pid(struct timestamp* start, struct timestamp* end,
				  unsigned int pid, unsigned long id1, unsigned long id2)
{
	struct timestamp* pos;
	unsigned int last_seqno = 0;

	for (pos = start; pos != end;  pos++) {
		/* check for for holes in the sequence number */
		if (last_seqno && last_seqno + 1 != pos->seq_no) {
			/* stumbled across a hole */
			return NULL;
		}
		last_seqno = pos->seq_no;

		/* only care about this PID */
		if (pos->pid == pid) {
			/* is  it the right one? */
			if (pos->event == id1 || pos->event == id2)
				return pos;
			else
				/* Don't allow unexptected IDs interleaved.
				 * Tasks are sequential, there shouldn't be
				 * anything else. */
				return NULL;
		}
	}
	return NULL;
}

static struct timestamp* accumulate_exec_time(
	struct timestamp* start, struct timestamp* end,
	unsigned int pid, unsigned long stop_id, uint64_t *sum)
{
	struct timestamp* pos = start;
	uint64_t exec_start;

	*sum = 0;

	while (1) {
		exec_start = pos->timestamp;

		/* find a suspension */
		pos = next_pid(pos + 1, end, pid, TS_LOCK_SUSPEND, stop_id);
		if (!pos)
			/* broken stream */
			return NULL;

		/* account for exec until pos */
		*sum += pos->timestamp - exec_start;

		if (pos->event == stop_id)
			/* no suspension */
			return pos;

		/* find matching resume */
		pos = next_pid(pos + 1, end, pid, TS_LOCK_RESUME, 0);
		if (!pos)
			/* broken stream */
			return NULL;

		/* must be a resume => start over */
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

	/* special case: take suspensions into account */
	if (first->event >= SUSPENSION_RANGE) {
		second = accumulate_exec_time(first, end,
					      first->pid,
					      first->event + 1, &exec_time);
	} else {
		second = next_pid(first + 1, end, first->pid,
				  first->event + 1, 0);
		if (second)
			exec_time = second->timestamp - first->timestamp;
	}
	if (second) {
		format_pair(first, second, exec_time);
		complete++;
	} else
		incomplete++;
}

static void find_event_by_eid(struct timestamp *first, struct timestamp* end)
{
	struct timestamp *second;
	uint64_t exec_time;

	second = find_second_ts(first, end);
	if (second) {
		exec_time = second->timestamp - first->timestamp;
		if (first->task_type != TSK_RT &&
			 second->task_type != TSK_RT && !want_best_effort)
			non_rt++;
		else {
			format_pair(first, second, exec_time);
			complete++;
		}
	} else
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

#define USAGE								\
	"Usage: ft2csv [-r] [-i] [-b] [-a CPU] [-o CPU] <event_name>  <logfile> \n" \
	"   -i: ignore interleaved  -- ignore samples if start "	\
	"and end are non-consecutive\n"					\
	"   -b: best effort         -- don't skip non-rt time stamps \n" \
	"   -r: raw binary format   -- don't produce .csv output \n"	\
	"   -a: avoid CPU           -- skip samples from a specific CPU\n" \
	"   -o: only CPU            -- skip all samples from other CPUs\n" \
	""

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "%s", USAGE);
	exit(1);
}

#define OPTS "ibra:o:pe"

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp *ts, *end;
	cmd_t id;
	int opt;
	char event_name[80];

	while ((opt = getopt(argc, argv, OPTS)) != -1) {
		switch (opt) {
		case 'i':
			want_interleaved = 0;
			fprintf(stderr, "Discarging interleaved samples.\n");
			break;
		case 'b':
			fprintf(stderr,"Not filtering samples from best-effort"
				" tasks.\n");
			want_best_effort = 1;
			break;
		case 'r':
			fprintf(stderr, "Generating binary (raw) output.\n");
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
		default:
			die("Unknown option.");
			break;
		}
	}

	if (argc - optind != 2)
		die("arguments missing");
	if (map_file(argv[optind + 1], &mapped, &size))
		die("could not map file");

	if (!str2event(argv[optind], &id)) {
		/* see if it is a short name */
		snprintf(event_name, sizeof(event_name), "%s_START",
			  argv[optind]);
		if (!str2event(event_name, &id))
			die("Unknown event!");
	}

	if (find_by_pid == AUTO_SELECT)
		find_by_pid = id <= PID_RECORDS_RANGE;

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

	if (id >= SINGLE_RECORDS_RANGE)
		show_single_records(ts, end, id);
	else
		show_id(ts, end, id);

	fprintf(stderr,
		"Total       : %10d\n"
		"Skipped     : %10d\n"
		"Avoided     : %10d\n"
		"Complete    : %10d\n"
		"Incomplete  : %10d\n"
		"Non RT      : %10d\n"
		"Interleaved : %10d\n",
		(int) count,
		skipped, avoided, complete,
		incomplete, non_rt,
		interleaved);

	return 0;
}
