#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>

#include "mapping.h"

static int _map_file(const char* filename, void **addr, size_t *size, int writable)
{
	struct stat info;
	int error = 0;
	int fd;
	int flags = writable ? MAP_SHARED : MAP_PRIVATE;

	error = stat(filename, &info);
	if (!error) {
		*size = info.st_size;
		if (info.st_size > 0) {
			fd = open(filename, O_RDWR);
			if (fd >= 0) {
				*addr = mmap(NULL, *size,
					     PROT_READ | PROT_WRITE,
					     flags,
					     fd, 0);
				if (*addr == MAP_FAILED)
					error = -1;
                else {
			/* tell kernel to start getting the pages */
			error = madvise(*addr, *size, MADV_SEQUENTIAL | MADV_WILLNEED);
			if (error) {
				perror("madvise");
			}
                }
				close(fd);
			} else
				error = fd;
		} else
			*addr = NULL;
	}
	return error;
}


int map_file(const char* filename, void **addr, size_t *size)
{
	return _map_file(filename, addr, size, 0);
}

int map_file_rw(const char* filename, void **addr, size_t *size)
{
	return _map_file(filename, addr, size, 1);
}
