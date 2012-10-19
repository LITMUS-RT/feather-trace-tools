/*    ft2sort -- Sort Feather-Trace events in a binary file by sequence number.
 *    Copyright (C) 2011,2012  B. Brandenburg.
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

#include <time.h>
#include <sys/time.h>

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include "mapping.h"

#include "timestamp.h"

static unsigned int holes      = 0;
static unsigned int non_monotonic = 0;
static unsigned int reordered  = 0;
static unsigned int aborted_moves = 0;

static int want_verbose = 0;

#define LOOK_AHEAD 1024
#define MAX_NR_NOT_IN_RANGE 5

#define MAX_CPUS UINT8_MAX

/* wall-clock time in seconds */
double wctime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec + 1E-6 * tv.tv_usec);
}

static uint32_t next_seq_number(uint32_t seqno)
{
	return seqno + 1;
}

struct timestamp* find_forward_by_seq_no(struct timestamp* start,
					 struct timestamp* end,
					 uint32_t seq_no)
{
	struct timestamp *pos;

	if (end > start + LOOK_AHEAD)
		end = start + LOOK_AHEAD;

	for (pos = start; pos < end; pos++)
		if (pos->seq_no == seq_no)
			return pos;

	return NULL;
}

static void mark_as_bad(struct timestamp *ts)
{
	if (want_verbose)
		printf("marking %s on cpu %u at %llu as bad\n",
		       event2str(ts->event), ts->cpu,
		       (unsigned long long) ts->timestamp);
	ts->event = UINT8_MAX;
	non_monotonic++;
}

static int in_range(uint32_t seqno, uint32_t candidate)
{
	uint32_t upper_bound = seqno + LOOK_AHEAD;
	uint32_t diff        = candidate - seqno;

	return (upper_bound < seqno && candidate < seqno && candidate < upper_bound) ||
		(candidate >= seqno && diff <  LOOK_AHEAD);
}

#define OVERFLOW_CUTOFF ((int32_t)UINT16_MAX / 2)

static int is_lower_seqno(int32_t candidate, int32_t min)
{
	/* compute difference in sequence numbers without overflow */
	int64_t delta = (int64_t) min - (int64_t) candidate;

	return (delta >= 0 && delta <= OVERFLOW_CUTOFF) ||
		(delta < -OVERFLOW_CUTOFF);
}

struct timestamp* find_lowest_seq_no(struct timestamp* start,
				     struct timestamp* end,
				     uint32_t seqno)
{
	struct timestamp *pos, *min = NULL;
	int nr_not_in_range = 0;

	if (end > start + LOOK_AHEAD)
		end = start + LOOK_AHEAD;

	for (pos = start; pos != end && (!min || min->seq_no != seqno); pos++) {
		/* pre-filter totally out-of-order samples */
		if (in_range(seqno, pos->seq_no) &&
		    (!min || is_lower_seqno(pos->seq_no, min->seq_no))) {
			min = pos;
		} else if (!in_range(seqno, pos->seq_no)) {
			if (++nr_not_in_range > MAX_NR_NOT_IN_RANGE)
				return NULL;
		}
	}
	return min;
}


static void move_record(struct timestamp* target, struct timestamp* pos)
{
	struct timestamp tmp, *prev;

	for (prev = target; prev < pos; prev++) {
		/* Refuse to violate task and CPU sequentiality: since CPUs and
		 * tasks execute sequentially, it makes no sense to move a
		 * timestamp before something recorded by the same task or
		 * CPU. */
		if (prev->cpu == pos->cpu ||
		    prev->pid == pos->pid) {
			/* Bail out before we cause more disturbance to the
			 * stream. */
			aborted_moves++;
			return;
		}
	}

	while (pos > target) {
		/* shift backwards */
		prev = pos - 1;

		tmp = *pos;
		*pos = *prev;
		*prev = tmp;

		pos = prev;
	}

	reordered++;
}

static void reorder(struct timestamp* start, struct timestamp* end)
{
	struct timestamp* pos, *tmp;
	uint32_t last_seqno = 0, expected_seqno;

	for (pos = start; pos != end;  pos++) {
		/* check for for holes in the sequence number */
		expected_seqno = next_seq_number(last_seqno);
		if (pos != start && expected_seqno != pos->seq_no) {
			tmp = find_lowest_seq_no(pos, end, expected_seqno);

			if (tmp && tmp != pos)
				/* Good, we found next-best candidate. */
				/* Move it to the right place. */
				move_record(pos, tmp);

			/* check if the sequence number lines up now */
			if (expected_seqno != pos->seq_no) {
				/* bad, there's a hole here */
				holes++;
				if (want_verbose)
					printf("HOLE: %u instead of %u\n",
					       pos->seq_no,
					       expected_seqno);
			}

		}
		last_seqno = pos->seq_no;
	}
}

static void pre_check_cpu_monotonicity(struct timestamp *start,
				       struct timestamp *end)
{
	struct timestamp *prev[MAX_CPUS];
	struct timestamp *pos[MAX_CPUS];
	struct timestamp *next;
	int i, outlier;
	uint8_t cpu;

	for (i = 0; i < MAX_CPUS; i++)
		prev[i] = pos[i] = NULL;

	for (next = start; next < end; next++) {
		if (next->event >= SINGLE_RECORDS_RANGE)
			continue;

		outlier = 0;
		cpu = next->cpu;

		/* Timestamps on each CPU should be monotonic. If there are
		 * "spikes" (high outliers) or "gaps" (low outliers), then the
		 * samples were disturbed by preemptions (not all samples are
		 * recorded with interrupts off). Samples disturbed in such
		 * ways create outliers; instead of filtering them later with
		 * statistical filters, we remove them while we can tell from
		 * context that they are anomalous observations.*/
		if (prev[cpu] && pos[cpu]) {
			/* check for spikes  -^- */
			if (prev[cpu]->timestamp < pos[cpu]->timestamp &&
			    pos[cpu]->timestamp >= next->timestamp &&
			    prev[cpu]->timestamp < next->timestamp) {
				outlier = 1;
			/* check for gaps -v- */
			} else if (prev[cpu]->timestamp >= pos[cpu]->timestamp &&
				   pos[cpu]->timestamp < next->timestamp &&
				   prev[cpu]->timestamp < next->timestamp) {
				outlier = 1;
			}
		}
		if (outlier) {
			/* pos[cpu] is an anomalous sample */
			mark_as_bad(pos[cpu]);
			pos[cpu] = next;
		} else {
			prev[cpu] = pos[cpu];
			pos[cpu] = next;
		}
	}
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
	return (bput(bget(0, q), 7) | bput(bget(1, q), 6) |
		bput(bget(2, q), 5) | bput(bget(3, q), 4) |
		bput(bget(4, q), 3) | bput(bget(5, q), 2) |
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

#define USAGE							\
	"Usage: ftsort [-e] <logfile> \n"			\
	"   -e: endianess swap      -- restores byte order \n"	\
	"   -s: simulate            -- don't overwrite file\n"  \
	"   -v: verbose             -- be chatty\n"		\
	"\n"							\
	"WARNING: Changes are permanent, unless -s is specified.\n"

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "%s", USAGE);
	exit(1);
}

#define OPTS "esv"

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp *ts, *end;
	int swap_byte_order = 0;
	int simulate = 0;
	int opt;
	double start, stop;

	while ((opt = getopt(argc, argv, OPTS)) != -1) {
		switch (opt) {
		case 'e':
			swap_byte_order = 1;
			break;
		case 's':
			simulate = 1;
			break;
		case 'v':
			want_verbose = 1;
			break;
		default:
			die("Unknown option.");
			break;
		}
	}

	if (argc - optind != 1)
		die("arguments missing");

	start = wctime();

	if (simulate) {
		if (map_file(argv[optind], &mapped, &size))
			die("could not RO map file");
	} else {
		if (map_file_rw(argv[optind], &mapped, &size))
			die("could not RW map file");
	}

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

	if (swap_byte_order)
		restore_byte_order(ts, end);

	pre_check_cpu_monotonicity(ts, end);
	reorder(ts, end);

	/* write back */
	if (simulate)
		fprintf(stderr, "Note: not writing back results.\n");
	else
		msync(ts, size, MS_SYNC | MS_INVALIDATE);

	stop = wctime();

	fprintf(stderr,
		"Total           : %10u\n"
		"Holes           : %10u\n"
		"Reordered       : %10u\n"
		"Non-monotonic   : %10u\n"
		"Seq. constraint : %10u\n"
		"Size            : %10.2f Mb\n"
		"Time            : %10.2f s\n"
		"Throughput      : %10.2f Mb/s\n",
		(unsigned int) count,
		holes, reordered, non_monotonic, aborted_moves,
		((double) size) / 1024.0 / 1024.0,
		(stop - start),
		((double) size) / 1024.0 / 1024.0 / (stop - start));

	return 0;
}
