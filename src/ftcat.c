#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "timestamp.h"

static int fd;

static void on_sigint(int sig)
{
	close(fd);
	exit(0);
}

static int enable_events(int fd, char* str) 
{
	unsigned long   id;
	unsigned long   cmd[3];

	if (!str2event(str, &id))
		return 0;

	cmd[0] = ENABLE_CMD;
	cmd[1] = id;
	cmd[2] = id + 1;
	return write(fd, cmd, 3 * sizeof(long)) == 3 * sizeof(long);
}


static void cat2stdout(int fd) 
{
	static char buf[4096];
	int rd;
	while ((rd = read(fd, buf, 4096)))
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
	signal(SIGINT,on_sigint);
	while (argc--) {
		if (!enable_events(fd, *argv)) {
			fprintf(stderr, "Enabling %s failed.\n", *argv);
			return 2;
		}
		argv++;
	}

	cat2stdout(fd);
	close(fd);
	return 0;
}

