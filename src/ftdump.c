/*
 *    ftdump -- Dump raw event data from a Feather-Trace event stream.
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

#include "mapping.h"

#include "timestamp.h"

static void dump(struct timestamp* ts, size_t count)
{
	struct timestamp *x;
	unsigned int last_seq = 0;
	const char* name;
	while (count--) {
		x = ts++;
		name = event2str(x->event);
		if (last_seq && last_seq + 1 != x->seq_no)
			printf("==== non-consecutive sequence number ====\n");
		last_seq = x->seq_no;
		if (name)
			printf("%-15s %-8s seq:%u cpu:%d timestamp:%llu\n",
			       name, task_type2str(x->task_type), x->seq_no, x->cpu, x->timestamp);
		else
			printf("event:%d seq:%u cpu:%d type:%s\n",
			       (int) x->event, x->seq_no, x->cpu, task_type2str(x->task_type));
	}
}

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

#define offset(type, member)  ((unsigned long) &((type *) 0)->member)

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp* ts;

	printf("struct timestamp:\n"
	       "\t size              = %3lu\n"
	       "\t offset(timestamp) = %3lu\n"
	       "\t offset(seq_no)    = %3lu\n"
	       "\t offset(cpu)       = %3lu\n"
	       "\t offset(event)     = %3lu\n"
	       "\t offset(task_type) = %3lu\n",
	       (unsigned long) sizeof(struct timestamp),
	       offset(struct timestamp, timestamp),
	       offset(struct timestamp, seq_no),
	       offset(struct timestamp, cpu),
	       offset(struct timestamp, event),
	       offset(struct timestamp, task_type));

	if (argc != 2)
		die("Usage: ftdump  <logfile>");
	if (map_file(argv[1], &mapped, &size))
		die("could not map file");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);

	dump(ts, count);
	return 0;
}
