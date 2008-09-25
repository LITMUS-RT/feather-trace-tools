#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mapping.h"

#include "timestamp.h"

static void dump(struct timestamp* ts, size_t count)
{
	struct timestamp *x;
	while (count--) {
		x = ts++;
		printf("event:%d seq:%u cpu:%d type:%d\n",
		       (int) x->event, x->seq_no, x->cpu, x->task_type);
	}
}

static void die(char* msg)
{
	if (errno)
		perror("error: ");
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

#define offset(type, member)  ((unsigned int) &((type *) 0)->member)

int main(int argc, char** argv)
{
	void* mapped;
	size_t size, count;
	struct timestamp* ts;

	printf("struct timestamp:\n"
	       "\t size              = %3lu\n"
	       "\t offset(timestamp) = %3u\n"
	       "\t offset(seq_no)    = %3u\n"
	       "\t offset(cpu)       = %3u\n"
	       "\t offset(event)     = %3u\n"
	       "\t offset(task_type) = %3u\n",
	       sizeof(struct timestamp),
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
