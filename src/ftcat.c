/*    ftcat -- Pump events from a Feather-Trace device to stdout.
 *    Copyright (C) 2007, 2008  B. Brandenburg.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>

#include "timestamp.h"

#define MAX_EVENTS 128

static int fd;
static int event_count = 0;
static cmd_t  ids[MAX_EVENTS];
static unsigned long total_bytes = 0;


static int disable_all(int fd)
{
	int disabled = 0;
	int i;

	fprintf(stderr, "Disabling %d events.\n", event_count);
	for (i = 0; i < event_count; i++)
		if (ioctl(fd, DISABLE_CMD, ids[i]) < 0)
			perror("ioctl(DISABLE_CMD)");
		else
			disabled++;

	return  disabled == event_count;
}

static int enable_event(int fd, char* str)
{
	cmd_t   *id;
	int err;

	id = ids + event_count;
	if (!str2event(str, id)) {
		errno = EINVAL;
		return 0;
	}
	event_count += 1;

	err = ioctl(fd, ENABLE_CMD, *id);

	if (err < 0)
		printf("ioctl(%d, %d, %d) => %d (errno: %d)\n", fd, (int) ENABLE_CMD, *id,
		       err, errno);

	return err == 0;
}


static void cat2stdout(int fd)
{
	static char buf[4096];
	int rd;
	while ((rd = read(fd, buf, 4096)) > 0) {
		total_bytes += rd;
		fwrite(buf, 1, rd, stdout);
	}
}


static void usage(void)
{
	fprintf(stderr,
		"Usage: ftcat <ft device> TS1 TS2 ...."
		"\n");
	exit(1);
}

static void shutdown(int sig)
{
	int ok;
	ok = disable_all(fd);
	if (!ok)
		fprintf(stderr, "disable_all: %m\n");
}

int main(int argc, char** argv)
{
	const char* trace_file;
	if (argc < 3)
		usage();

	trace_file = argv[1];
	fd = open(trace_file, O_RDWR);
	if (fd < 0) {
		perror("could not open feathertrace");
		return 1;
	}
	argc -= 2;
	argv += 2;
	signal(SIGINT, shutdown);
	signal(SIGUSR1, shutdown);
	signal(SIGTERM, shutdown);
	while (argc--) {
		if (!enable_event(fd, *argv)) {
			fprintf(stderr, "Enabling %s failed: %m\n", *argv);
			return 2;
		}
		argv++;
	}

	cat2stdout(fd);
	close(fd);
	fflush(stdout);
	fprintf(stderr, "%s: %lu bytes read.\n", trace_file, total_bytes);
	return 0;
}

