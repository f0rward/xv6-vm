#ifndef _BUDDY_H_
#define _BUDDY_H_
#include "pmap.h"

#define MAX_ORDER  11

typedef struct free_area {
	page_list_head_t free_list;
	unsigned long nr_free;
} free_area_t;

void init_memmap(struct Page * base, unsigned long nr);

extern struct Page * mem_map;
struct Page * __alloc_pages(int nr);
void __free_pages(struct Page * page, int nr);
struct Page * alloc_pages_bulk(int order);
void free_pages_bulk(struct Page * page, int order);

#endif
