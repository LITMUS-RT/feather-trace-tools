#ifndef EHEAP_H
#define EHEAP_H

#include <limits.h>
#include "heap.h"

int earlier_event(struct heap_node* _a, struct heap_node* _b);

struct heap* heapify_events(struct st_event_record *ev, unsigned int count);


#endif
