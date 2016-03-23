#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "sched_trace.h"
#include "eheap.h"
#include "load.h"

static int map_file(const char* filename, void **addr, size_t *size)
{
	struct stat info;
	int error = 0;
	int fd;

	error = stat(filename, &info);
	if (!error) {
		*size = info.st_size;
		if (info.st_size > 0) {
			fd = open(filename, O_RDONLY);
			if (fd >= 0) {
				*addr = mmap(NULL, *size,
					     PROT_READ | PROT_WRITE,
					     MAP_PRIVATE, fd, 0);
				if (*addr == MAP_FAILED)
					error = -1;
				close(fd);
			} else
				error = fd;
		} else
			*addr = NULL;
	}
	return error;
}

int map_trace(const char *name, void **start, void **end, size_t *size)
{
	int ret;

	ret = map_file(name, start, size);
	if (!ret)
		*end = *start + *size;
	return ret;
}


static struct heap* heap_from_file(char* file, unsigned int* count)
{
	size_t s;
	struct st_event_record *rec, *end;
	if (map_trace(file, (void**) &rec, (void**) &end, &s) == 0) {
		*count = ((unsigned int)((char*) end - (char*) rec))
			/ sizeof(struct st_event_record);
		return heapify_events(rec, *count);
	} else
		fprintf(stderr, "mmap: %m (%s)\n", file);
	return NULL;
}

struct heap* load(char **files, int no_files, unsigned int *count)
{
	int i;
	unsigned int c;
	struct heap *h = NULL, *h2;
	*count = 0;
	for (i = 0; i < no_files; i++) {
		h2 = heap_from_file(files[i], &c);
		if (!h2)
			/* potential memory leak, don't care */
			return NULL;
		if (h)
			heap_union(earlier_event, h, h2);
		else
			h = h2;
		*count += c;
	}
	return h;
}



struct task tasks[MAX_TASKS];
struct evlink *sys_events = NULL;
struct evlink **sys_next  = &sys_events;
u64 time0 = 0;
u32 g_max_task = MAX_TASKS;
u32 g_min_task = 0;

void init_tasks(void)
{
	int i;

	for (i = 0; i < MAX_TASKS; i++) {
		tasks[i] = (struct task) {0, 0, NULL, NULL, NULL, NULL};
		tasks[i].next = &tasks[i].events;
	}
}

void crop_events(struct task* t, double min, double max)
{
	struct evlink **p;
	double time;
	p = &t->events;
	while (*p) {
		time = evtime((*p)->rec);
		if (time < min || time > max)
			*p = (*p)->next;
		else
			p = &(*p)->next;
	}
}

void crop_events_all(double min, double max)
{
	struct task* t;
	for_each_task(t)
		crop_events(t, min, max);
}

struct task* by_pid(int pid)
{
	struct task* t;
	if (!pid)
		return NULL;
	/* slow, don't care for now */
	for (t = tasks; t < tasks + MAX_TASKS; t++) {
		if (!t->pid) /* end, allocate */
			t->pid = pid;
		if (t->pid == pid)
			return t;
	}
	return NULL;
}

u32 count_tasks(void)

{
	struct task* t;
	u32 i = 0;
	for_each_task(t)
		i++;
	return i;
}


void split(struct heap* h, unsigned int count, int find_time0)
{
	struct evlink *lnk = malloc(count * sizeof(struct evlink));
	struct heap_node *hn;
	u64 time;
	struct st_event_record *rec;
	struct task* t;

	if (!lnk) {
		perror("malloc");
		return;
	}

	while ((hn = heap_take(earlier_event, h))) {
		rec = heap_node_value(hn);
		time =  event_time(rec);
		if (find_time0 && !time0 && time)
			time0 = time;
		t = by_pid(rec->hdr.pid);
		switch (rec->hdr.type) {
		case ST_PARAM:
			if (t)
				t->param = rec;
			else
				fprintf(stderr, "Dropped ST_PARAM record "
					"for PID %d.\n", rec->hdr.pid);
			break;
		case ST_NAME:
			if (t)
				t->name = rec;
			else
				fprintf(stderr, "Dropped ST_NAME record "
					"for PID %d.\n", rec->hdr.pid);
			break;
		default:
			lnk->rec = rec;
			lnk->next = NULL;
			if (t) {
				*(t->next) = lnk;
				t->next = &lnk->next;
				t->no_events++;
			} else {
				*(sys_next) = lnk;
				sys_next    = &lnk->next;
			}
			lnk++;
			break;
		}
	}
}

int tsk_cpu(struct task *t)
{
	if (t->param)
		return t->param->data.param.partition;
	else
		return -1;
}

const char* tsk_name(struct task* t)
{
	if (t->name)
		return t->name->data.name.cmd;
	else
		return "<unknown>";
}

u32 per(struct task* t)
{
	if (t->param)
		return t->param->data.param.period;
	else
		return 0;
}

u32 exe(struct task* t)
{
	if (t->param)
		return t->param->data.param.wcet;
	else
		return 0;
}

u32 idx(struct task* t)
{
	return (t - tasks);
}
