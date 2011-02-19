/*    ft2sort -- Sort Feather-Trace events in a binary file by sequence number.
 *    Copyright (C) 2011  B. Brandenburg.
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
static unsigned int reordered  = 0;

#define LOOK_AHEAD 1000

/* wall-clock time in seconds */
double wctime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec + 1E-6 * tv.tv_usec);
}


static struct timestamp* find_lowest_seq_no(struct timestamp* start,
					    struct timestamp* end,
					    unsigned int seqno)
{
	struct timestamp *pos, *min = start;

	if (end > start + LOOK_AHEAD)
		end = start + LOOK_AHEAD;

	for (pos = start; pos != end && min->seq_no != seqno; pos++)
		if (pos->seq_no < min->seq_no)
			min = pos;
	return min;
}


static void move_record(struct timestamp* target, struct timestamp* pos)
{
	struct timestamp tmp, *prev;

	while (pos > target) {
		/* shift backwards */
		tmp = *pos;
		prev = pos - 1;

		*pos = *prev;
		*prev = tmp;

		pos = prev;
	}
}

static void reorder(struct timestamp* start, struct timestamp* end)
{
	struct timestamp* pos, *tmp;
	unsigned int last_seqno = 0;

	for (pos = start; pos != end;  pos++) {
		/* check for for holes in the sequence number */
		if (last_seqno && last_seqno + 1 != pos->seq_no) {
			tmp = find_lowest_seq_no(pos, end, last_seqno + 1);
			if (tmp->seq_no == last_seqno + 1)
				/* Good, we found it. */
				/* Move it to the right place. */
				reordered++;
		        else {
				/* bad, there's a hole here */
				holes++;
				fprintf(stderr, "HOLE: %u instead of %u\n", tmp->seq_no, last_seqno + 1);
			}
			move_record(pos, tmp);
		}
		last_seqno = pos->seq_no;
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
	"\n"							\
	"WARNING: Changes are permanent.\n"

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "%s", USAGE);
	exit(1);
}

#define OPTS "e"

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp *ts, *end;
	int swap_byte_order = 0;
	int opt;
	double start, stop;

	while ((opt = getopt(argc, argv, OPTS)) != -1) {
		switch (opt) {
		case 'e':
			swap_byte_order = 1;
			break;
		default:
			die("Unknown option.");
			break;
		}
	}

	if (argc - optind != 1)
		die("arguments missing");

	start = wctime();

	if (map_file_rw(argv[optind], &mapped, &size))
		die("could not map file");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

	if (swap_byte_order)
		restore_byte_order(ts, end);

	reorder(ts, end);

	/* write back */
	msync(ts, size, MS_SYNC | MS_INVALIDATE);

	stop = wctime();

	fprintf(stderr,
		"Total       : %10d\n"
		"Holes       : %10d\n"
		"Reordered   : %10d\n"
		"Size        : %10.2f Mb\n"
		"Time        : %10.2f s\n"
		"Throughput  : %10.2f Mb/s\n",
		(int) count,
		holes, reordered,
		((double) size) / 1024.0 / 1024.0,
		(stop - start),
		((double) size) / 1024.0 / 1024.0 / (stop - start));

	return 0;
}
