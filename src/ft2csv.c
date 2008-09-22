#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mapping.h"

#include "timestamp.h"

int no_interleaved = 1;

static unsigned int complete   = 0;
static unsigned int incomplete = 0;
static unsigned int filtered   = 0;
static unsigned int skipped    = 0;
static unsigned int non_rt     = 0;
static unsigned int interleaved = 0;

static unsigned long long threshold = 2700 * 1000; /* 1 ms == 1 full tick */
//static unsigned long long threshold = 2700 * 50; /* 1 ms == 1 full tick */

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
		if (no_interleaved)
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
			 second->task_type != TSK_RT)
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
	struct timestamp *ts, *end;	
	cmd_t id;

	if (argc != 3)
		die("Usage: ft2csv  <event_name>  <logfile>");
	if (map_file(argv[2], &mapped, &size))
		die("could not map file");

	if (!str2event(argv[1], &id))
		die("Unknown event!");

	ts    = (struct timestamp*) mapped;
	count = size / sizeof(struct timestamp);
	end   = ts + count;

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
