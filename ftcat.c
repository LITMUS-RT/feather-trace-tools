#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "timestamp.h"


static int enable_events(int fd, char* str) 
{
	long   id;
	long   cmd[3];

	if        (!strcmp(str, "sched")) {
		id = TS_SCHED_START;		
	} else if (!strcmp(str, "tick")) {
	        id = TS_TICK_START;
	} else if (!strcmp(str, "plug_tick")) {
	        id = TS_PLUGIN_TICK_START;
	} else if (!strcmp(str, "plug_sched")) {
	        id = TS_PLUGIN_SCHED_START;
	} else
		return 0;

	cmd[0] = ENABLE_CMD;
	cmd[1] = id;
	cmd[2] = id + 2;
	return write(fd, cmd, 3 * sizeof(long)) == 3 * sizeof(long);
}



static void cat2stdout(int fd) 
{
	static char buf[4096];
	int rd;
	while (rd = read(fd, buf, 4096))
		fwrite(buf, 1, rd, stdout);	
}


static void usage(void)
{
	fprintf(stderr, "Usage: ftcat <ft device> TS1 TS2 ....\n\n");
	exit(1);
}

int main(int argc, char** argv) 
{
	int fd;

	if (argc < 3) 
		usage();

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("could not open feathertrace");
		return 1;
	}
	argc -= 2;
	argv += 2;
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

