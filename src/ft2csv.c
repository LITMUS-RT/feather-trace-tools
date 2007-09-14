#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mapping.h"

#include "timestamp.h"

static unsigned int incomplete = 0;
static unsigned int filtered   = 0;
static unsigned int skipped    = 0;

static unsigned long long threshold = 2700 * 1000; /* 1 ms == 1 full tick */

static struct timestamp* next(struct timestamp** pos, size_t* count, int cpu)
{
	while (*count--)
		if ((++(*pos))->cpu == cpu)
			return *pos;       
	return NULL;
}

static struct timestamp* next_id(struct timestamp** pos, size_t* count, 
				 int cpu, unsigned long id) 
{
	struct timestamp* ret;
	while ((ret = next(pos, count, cpu)) && (*pos)->event != id);
	return ret;
}

static int find_pair(struct timestamp* start, 
		     struct timestamp** end,
		     size_t count)
{
	struct timestamp *pos = start;
	/* convention: the end->event is start->event + 1 */
	*end = next_id(&pos, &count, start->cpu, start->event + 1);
	return *end != NULL;
}

static void show_csv(struct timestamp* ts, size_t count)
{
	struct timestamp* start = ts;
	struct timestamp* stop;

	if (find_pair(start, &stop, count)) {
		if (stop->timestamp - start->timestamp > threshold)
			filtered++;
		else
			printf("%llu, %llu, %llu\n",
			       start->timestamp, stop->timestamp, 
			       stop->timestamp - start->timestamp);
	} else
		incomplete++;
	
}

/*static void show_all_per_cpu(struct timestamp* ts, size_t count)
{
	struct timestamp* _ts = ts;
	size_t _count = count;
	int cpu;
	
	for (cpu = 0; cpu < 4; cpu++) {
		count = _count;
		ts    = _ts;
		while (count--) {
			if (ts->cpu == cpu)
				show(ts, count);	
			ts++;
		}
	}
}*/

static void show_id(struct timestamp* ts, size_t count,  unsigned long id)
{
	while (ts->event != id + 1 && count--) {
		skipped++;
		ts++;
	}
	if (!count)
		return;
	while (count--)
		if (ts->event == id)
			show_csv(ts++, count);
		else
			ts++;
}


static void dump(struct timestamp* ts, size_t count,  unsigned long id)
{
	while (count--)
		printf("%lu\n", (ts++)->event);
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
	unsigned long id;

	if (argc != 3)
		die("Usage: ft2csv  <event_name>  <logfile>");
	if (map_file(argv[2], &mapped, &size))
		die("could not map file");

	if (!str2event(argv[1], &id))
		die("Unknown event!");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);		

	show_id(ts, count, id);

	fprintf(stderr,
		"Skipped   : %d\n"
		"Incomplete: %d\n"
		"Filtered  : %d\n", 
		skipped, incomplete, filtered);

	return 0;
}
