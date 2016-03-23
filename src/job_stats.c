#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "load.h"
#include "sched_trace.h"
#include "eheap.h"

/* limit search window in case of missing completions */
#define MAX_COMPLETIONS_TO_CHECK 20

int want_ms = 0;

static double nano_to_ms(int64_t ns)
{
	return ns * 1E-6;
}

static void print_stats(
	struct task* t,
	struct st_event_record *release,
	struct st_event_record *completion)
{
	int64_t lateness;
	u64 response;

	lateness  = completion->data.completion.when;
	lateness -= release->data.release.deadline;
	response  = completion->data.completion.when;
	response -= release->data.release.release;

	if (want_ms)
		printf(" %5u, %5u, %10.2f, %10.2f, %8d, %10.2f, %10.2f, %7d"
			", %10.2f\n",
		       release->hdr.pid,
		       release->hdr.job,
		       nano_to_ms(per(t)),
		       nano_to_ms(response),
		       lateness > 0,
		       nano_to_ms(lateness),
		       lateness > 0 ? nano_to_ms(lateness) : 0,
		       completion->data.completion.forced,
		       nano_to_ms(completion->data.completion.exec_time));
	else
		printf(" %5u, %5u, %10llu, %10llu, %8d, %10lld, %10lld, %7d"
			", %10llu\n",
		       release->hdr.pid,
		       release->hdr.job,
		       (unsigned long long) per(t),
		       (unsigned long long) response,
		       lateness > 0,
		       (long long) lateness,
		       lateness > 0 ? (long long) lateness : 0,
		       completion->data.completion.forced,
		       (unsigned long long) completion->data.completion.exec_time);
}

static void print_task_info(struct task *t)
{
	if (want_ms)
		printf("# task NAME=%s PID=%d COST=%.2f PERIOD=%.2f CPU=%d\n",
		       tsk_name(t),
		       t->pid,
		       nano_to_ms(exe(t)),
		       nano_to_ms(per(t)),
		       tsk_cpu(t));
	else
		printf("# task NAME=%s PID=%d COST=%lu PERIOD=%lu CPU=%d\n",
		       tsk_name(t),
		       t->pid,
		       (unsigned long) exe(t),
		       (unsigned long) per(t),
		       tsk_cpu(t));
}

static void usage(const char *str)
{
	fprintf(stderr,
		"\n  USAGE\n"
		"\n"
		"    st_job_stats [opts] <file.st>+\n"
		"\n"
		"  OPTIONS\n"
		"     -r         -- skip jobs prior to task-system release\n"
		"     -m         -- output milliseconds (default: nanoseconds)\n"
		"     -p PID     -- show only data for the task with the given PID\n"
		"     -n NAME    -- show only data for the task(s) with the given NAME\n"
		"     -t PERIOD  -- show only data for the task(s) with the given PERIOD\n"
		"\n\n"
		);
	if (str) {
		fprintf(stderr, "Aborted: %s\n", str);
		exit(1);
	} else {
		exit(0);
	}
}

#define OPTSTR "rmp:n:t:h"

int main(int argc, char** argv)
{
	unsigned int count;
	struct heap *h;

	struct task *t;
	struct evlink *e, *pos;
	struct st_event_record *rec;

	int wait_for_release = 0;
	u64 sys_release = 0;

	unsigned int pid_filter = 0;
	const char* name_filter = 0;
	u32 period_filter = 0;

	int opt;

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 'r':
			wait_for_release = 1;
			break;
		case 'm':
			want_ms = 1;
			break;
		case 'p':
			pid_filter = atoi(optarg);
			if (!pid_filter)
				usage("Invalid PID.");
			break;
		case 't':
			period_filter = atoi(optarg);
			if (!period_filter)
				usage("Invalid period.");
			break;
		case 'n':
			name_filter = optarg;
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

	if (want_ms)
		period_filter *= 1000000; /* ns per ms */

	h = load(argv + optind, argc - optind, &count);
	if (!h)
		return 1;

	init_tasks();
	split(h, count, 1);

	if (wait_for_release) {
		rec = find_sys_event(ST_SYS_RELEASE);
		if (rec)
			sys_release = rec->data.sys_release.release;
		else {
			fprintf(stderr, "Could not find task system "
				"release time.\n");
			exit(1);
		}
	}

	/* print header */
	printf("#%5s, %5s, %10s, %10s, %8s, %10s, %10s, %7s, %10s\n",
	       "Task",
	       "Job",
	       "Period",
	       "Response",
	       "DL Miss?",
	       "Lateness",
	       "Tardiness",
	       "Forced?",
	       "ACET");

	/* print stats for each task */
	for_each_task(t) {
		if (pid_filter && pid_filter != t->pid)
			continue;
		if (name_filter && strcmp(tsk_name(t), name_filter))
			continue;
		if (period_filter && period_filter != per(t))
			continue;

		print_task_info(t);
		for_each_event(t, e) {
			rec = e->rec;
			if (rec->hdr.type == ST_RELEASE &&
			    (!wait_for_release ||
			     rec->data.release.release >= sys_release)) {
				pos  = e;
				count = 0;
				while (pos && count < MAX_COMPLETIONS_TO_CHECK) {
					find(pos, ST_COMPLETION);
					if (pos->rec->hdr.job == rec->hdr.job) {
						print_stats(t, rec, pos->rec);
						break;
					} else {
						pos = pos->next;
						count++;
					}
				}

			}
		}
	}

	return 0;
}
