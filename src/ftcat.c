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

#include "migration.h" /* from liblitmus */
#include "timestamp.h"

#define MAX_EVENTS 128

int verbose = 0;
#define vprintf(fmt, args...) if (verbose) { printf(fmt, ## args); }

static int fd;
static int event_count = 0;
static cmd_t  ids[MAX_EVENTS];
static unsigned long total_bytes = 0;
static unsigned long stop_after_bytes = 0;

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
		fprintf(stderr, "ioctl(%d, %d, %d) => %d (errno: %d)\n", fd, (int) ENABLE_CMD, *id,
		       err, errno);

	return err == 0;
}

static int calibrate_cycle_offsets(int fd)
{
	int cpu, err;
	int max_cpus = sysconf(_SC_NPROCESSORS_CONF);

	for (cpu = 0; cpu < max_cpus; cpu++) {
		err = be_migrate_to_cpu(cpu);
		if (!err) {
			vprintf("Calibrating CPU %d...", cpu);
			err = ioctl(fd, CALIBRATE_CMD, verbose);
			if (err) {
				vprintf("\n");
				fprintf(stderr, "ioctl(CALIBRATE_CMD) => %d (errno: %d, %m)\n", err, errno);
				return 0;
			} else
				vprintf(" done.\n");
		} else {
			fprintf(stderr, "Could not migrate to CPU %d (%m).\n", cpu);
		}
	}
	return 1;
}


static void cat2stdout(int fd)
{
	static char buf[4096];
	int rd;
	while ((rd = read(fd, buf, 4096)) > 0) {
		total_bytes += rd;
		fwrite(buf, 1, rd, stdout);
		if (stop_after_bytes && total_bytes >= stop_after_bytes)
			break;
	}
}

static void ping(const char* fname)
{
	FILE* f = fopen(fname, "a");
	if (f) {
		fprintf(f, "%d\n", getpid());
		fclose(f);
	}
}

static void _usage(void)
{
	fprintf(stderr,
		"Usage: ftcat [OPTIONS] <ft device> TS1 TS2 ....\n"
		"\nOptions:\n"
		"   -s SIZE   --  stop tracing afer recording SIZE bytes\n"
		"   -c        --  calibrate the CPU cycle counter offsets\n"
		"   -p FILE   --  ping: write PID to FILE after initialization\n"
		"   -v        --  enable verbose output\n"
		"\n");
	exit(1);
}

#define usage(fmt, args...) do { fprintf(stderr, "Error: " fmt "\n", ## args); _usage(); }  while (0)

static void shutdown(int sig)
{
	int ok;
	ok = disable_all(fd);
	if (!ok)
		fprintf(stderr, "disable_all: %m\n");
}

#define OPTSTR "s:cvp:"

int main(int argc, char** argv)
{
	int opt;
	int want_calibrate = 0;

	const char* trace_file;
	const char* ping_file = NULL;

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 's':
		        stop_after_bytes = atol(optarg);
			if (stop_after_bytes == 0)
				usage("invalid size (%s)", optarg);
			break;
		case 'c':
			want_calibrate = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'p':
			ping_file = optarg;
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

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage("Argument missing.");

	trace_file = argv[0];
	fd = open(trace_file, O_RDWR);
	if (fd < 0) {
		usage("could not open feathertrace device (%s): %m", trace_file);
		return 1;
	}

	if (want_calibrate && !calibrate_cycle_offsets(fd)) {
		fprintf(stderr, "Calibrating %s failed: %m\n", *argv);
		return 3;
	}

	argc -= 1;
	argv += 1;
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

	if (ping_file)
		ping(ping_file);

	cat2stdout(fd);
	close(fd);
	fflush(stdout);
	fprintf(stderr, "%s: %lu bytes read.\n", trace_file, total_bytes);
	return 0;
}

