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

int main(int argc, char** argv) 
{
	void* mapped;
	size_t size, count;
	struct timestamp* ts;	

	if (argc != 2)
		die("Usage: ftdump  <logfile>");
	if (map_file(argv[1], &mapped, &size))
		die("could not map file");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);		

	dump(ts, count);
	return 0;
}
