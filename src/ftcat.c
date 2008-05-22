#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "timestamp.h"

#define MAX_EVENTS 128

static int fd;
static int event_count = 1;
static cmd_t  ids[MAX_EVENTS];

static int disable_all(int fd)
{
	int ret, size;
	ids[0] = DISABLE_CMD;
	fprintf(stderr, "Disabling %d events.\n", event_count - 1);
	size = event_count * sizeof(cmd_t);
	ret  = write(fd, ids, size);
	//fprintf(stderr, "write = %d, meant to write %d (%m)\n", ret, size);
	return size == ret;
}

static int enable_events(int fd, char* str) 
{
	cmd_t   *id;
	cmd_t   cmd[3];

	id = ids + event_count;
	if (!str2event(str, id))
		return 0;

	event_count += 2;
	id[1]  = id[0] + 1;
	cmd[0] = ENABLE_CMD;
	cmd[1] = id[0];
	cmd[2] = id[1];
	return write(fd, cmd, 3 * sizeof(cmd_t)) == 3 * sizeof(cmd_t);
}


static void cat2stdout(int fd) 
{
	static char buf[4096];
	int rd;
	while ((rd = read(fd, buf, 4096)) > 0)
		fwrite(buf, 1, rd, stdout);
}


static void usage(void)
{
	fprintf(stderr, 
		"Usage: ftcat <ft device> TS1 TS2 ....\n\n"
		//		"where TS1, TS2, ... is one of "
		//		" sched, tick, plug_tick, plug_sche"
		"\n");
	exit(1);
}

static void on_sigint(int sig)
{
	close(fd);
	exit(0);
}

static void on_sigusr1(int sig)
{
	int ok;
	ok = disable_all(fd);
	if (!ok)
		fprintf(stderr, "disable_all: %m\n");
}

int main(int argc, char** argv) 
{
	if (argc < 3) 
		usage();
	
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("could not open feathertrace");
		return 1;
	}
	argc -= 2;
	argv += 2;
	signal(SIGINT, on_sigint);
	signal(SIGUSR1, on_sigusr1);
	while (argc--) {
		if (!enable_events(fd, *argv)) {
			fprintf(stderr, "Enabling %s failed: %m\n", *argv);
			return 2;
		}
		argv++;
	}

	cat2stdout(fd);
	close(fd);
	return 0;
}

