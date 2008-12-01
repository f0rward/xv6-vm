#ifndef _BUDDY_H_
#define _BUDDY_H_
#include "pmap.h"

#define MAX_ORDER  11

typedef struct free_area {
	page_list_head_t free_list;
	unsigned long nr_free;
} free_area_t;

void init_memmap(struct Page * base, unsigned long nr);

#endif
