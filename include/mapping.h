#ifndef _MAPPING_H_
#define _MAPPING_H_

int map_file(const char* filename, void **addr, size_t *size);

int map_file_rw(const char* filename, void **addr, size_t *size);

#endif

