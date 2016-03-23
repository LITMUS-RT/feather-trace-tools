#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "load.h"
#include "sched_trace.h"
#include "eheap.h"

static void usage(const char *str)
{
	fprintf(stderr,
		"\n  USAGE\n"
		"\n"
		"    stdump [opts] <file.st>+\n"
		"\n"
		"  OPTIONS\n"
		"     -r         -- find task system release and exit\n"
		"     -f         -- use first non-zero event as system release\n"
		"                   if no system release event is found\n"
		"     -c         -- display a count of the number of events\n"
		"\n\n"
		);
	if (str) {
		fprintf(stderr, "Aborted: %s\n", str);
		exit(1);
	} else {
		exit(0);
	}
}

#define OPTSTR "rcfh"

int main(int argc, char** argv)
{
	unsigned int count;
	struct heap *h;
	struct heap_node *hn, *first = NULL;
	u64 time;
	struct st_event_record *rec;
	int find_release = 0;
	int show_count = 0;
	int use_first_nonzero = 0;
	int opt;

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 'r':
			find_release = 1;
			break;
		case 'c':
			show_count = 1;
			break;
		case 'f':
			use_first_nonzero = 1;
			break;
		case 'h':
			usage(NULL);
			break;
		case ':':
			usage("Argument missing.");
			break;
		case '?':
		default:
			usage("Bad argument.");
			break;
		}
	}


	h = load(argv + optind, argc - optind, &count);
	if (!h)
		return 1;
	if (show_count)
		printf("Loaded %u events.\n", count);
	while ((hn = heap_take(earlier_event, h))) {
		time =  event_time(heap_node_value(hn));
		if (time != 0 && !first)
			first = hn;
		time /= 1000000; /* convert to milliseconds */
		if (!find_release) {
			printf("[%10llu] ", (unsigned long long) time);
			print_event(heap_node_value(hn));
		} else {
			rec = heap_node_value(hn);
			if (rec->hdr.type == ST_SYS_RELEASE) {
				printf("%6.2fms\n", rec->data.raw[1] / 1000000.0);
				find_release = 0;
				break;
			}
		}
	}
	if (find_release && use_first_nonzero && first) {
		rec = heap_node_value(first);
		printf("%6.2fms\n", event_time(rec) / 1000000.0);
	}

	return 0;
}
