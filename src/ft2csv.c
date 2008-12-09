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

static int want_interleaved    = 1;
static int want_best_effort    = 0;

static unsigned int complete   = 0;
static unsigned int incomplete = 0;
static unsigned int filtered   = 0;
static unsigned int skipped    = 0;
static unsigned int non_rt     = 0;
static unsigned int interleaved = 0;

static unsigned long long threshold = 2700 * 1000; /* 1 ms == 1 full tick */

static struct timestamp* next(struct timestamp* start, struct timestamp* end,
			      int cpu)
{
	struct timestamp* pos;
	for (pos = start; pos != end && pos->cpu != cpu; pos++);
	return pos != end ? pos : NULL;
}

static struct timestamp* next_id(struct timestamp* start, struct timestamp* end,
				 int cpu, unsigned long id, unsigned long stop_id)
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

static struct timestamp* find_second_ts(struct timestamp* start, struct timestamp* end)
{
	/* convention: the end->event is start->event + 1 */
	return next_id(start + 1, end, start->cpu, start->event + 1, start->event);
}

static void show_csv(struct timestamp* first, struct timestamp *end)
{
	struct timestamp *second;

	second = find_second_ts(first, end);
	if (second) {
		if (second->timestamp - first->timestamp > threshold)
			filtered++;
		else if (first->task_type != TSK_RT &&
			 second->task_type != TSK_RT && !want_best_effort)
			non_rt++;
		else {
			printf("%llu, %llu, %llu\n",
			       (unsigned long long) first->timestamp,
			       (unsigned long long) second->timestamp,
			       (unsigned long long)
			       (second->timestamp - first->timestamp));
			complete++;
		}
	} else
		incomplete++;

}

static inline uint64_t bget(int x, uint64_t quad)

{
	return (((0xffll << 8 * x) & quad) >> 8 * x);
}

static inline uint64_t bput(uint64_t b, int pos)
{
	return (b << 8 * pos);
}

static inline uint64_t ntohx(uint64_t q)
{
	return (bput(bget(0, q), 7) | bput(bget(1, q), 6) | bput(bget(2, q), 5) |
		bput(bget(3, q), 4) | bput(bget(4, q), 3) | bput(bget(5, q), 2) |
		bput(bget(6, q), 1) | bput(bget(7, q), 0));
}

static void restore_byte_order(struct timestamp* start, struct timestamp* end)
{
	struct timestamp* pos = start;
	while (pos !=end) {
		pos->timestamp = ntohx(pos->timestamp);
		pos->seq_no    = ntohl(pos->seq_no);
		pos++;
	}
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

#define USAGE \
	"Usage: ft2csv [-e]  [-i] <event_name>  <logfile> \n"	\
	"   -e: endianess swap      -- restores byte order \n"	\
	"   -i: ignore interleaved  -- ignore samples if start " \
	"and end are non-consecutive\n"				\
	""

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "%s", USAGE);
	exit(1);
}

#define OPTS "eib"

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp *ts, *end;
	cmd_t id;
	int swap_byte_order = 0;
	int opt;

	/*
	printf("%llx -> %llx\n", 0xaabbccddeeff1122ll,
	       ntohx(0xaabbccddeeff1122ll));
	printf("%x -> %x\n", 0xaabbccdd,
	       ntohl(0xaabbccdd));
	*/
	while ((opt = getopt(argc, argv, OPTS)) != -1) {
		switch (opt) {
		case 'e':
			swap_byte_order = 1;
			break;
		case 'i':
			want_interleaved = 0;
			break;
		case 'b':
			want_best_effort = 1;
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

	if (!str2event(argv[optind], &id))
		die("Unknown event!");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

	if (swap_byte_order)
		restore_byte_order(ts, end);
	show_id(ts, end, id);

	fprintf(stderr,
		"Total       : %10d\n"
		"Skipped     : %10d\n"
		"Complete    : %10d\n"
		"Incomplete  : %10d\n"
		"Filtered    : %10d\n"
		"Non RT      : %10d\n"
		"Interleaved : %10d\n",
		(int) count,
		skipped, complete,
		incomplete, filtered, non_rt,
		interleaved);

	return 0;
}
