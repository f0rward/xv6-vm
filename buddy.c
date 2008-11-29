#include "buddy.h"

free_area_t free_area[MAX_ORDER];
struct Page * mem_map;
int FreeAreaSize[MAX_ORDER] = {1,2,4,8,16,32,64,128,256,512,1024};

// check if pointer page indicates the first page frame of a block of order_size pages
// if so , return 1, else return 0
int page_is_buddy(struct Page * page, int order)
{
	if (page->property == order)
		return 1;
	return 0;
}

void init_memmap(struct Page * base, unsigned long nr)
{
	int order, size;
	mem_map = base;
	while (nr) {
		for (order = 10; order >=0; order --) {
		}
	}
}

// implement the buddy system strategy for freeing page frames
struct Page * alloc_pages_bulk(int order)
{
	int isalloc = 0;
	int current_order, size = 0;

	struct Page * page = NULL, * buddy = NULL;

	// try to find a suitable block in the buddy system
	for (current_order = order; current_order < MAX_ORDER; current_order ++) {
		if (!LIST_EMPTY(&(free_area[current_order].free_list))) {
			isalloc = 1;
			break;
		}
	}

	if (!isalloc)
		return NULL;
	else {
		page = LIST_FIRST(&(free_area[current_order].free_list));
		LIST_REMOVE(page, lru);
		page->property = 0;
		free_area[current_order].nr_free --;
	}

	size = 1 << current_order;
	while (current_order > order) {
		current_order --;
		size >>= 1;
		buddy = page + size;
		LIST_INSERT_HEAD(&(free_area[current_order].free_list), buddy, lru);
		buddy->property = current_order;
		free_area[current_order].nr_free ++;
	}

	return page;
}

// implement the buddy system strategy for freeing page frames
void free_pages_bulk(struct Page * page, int order)
{
	int size = 1 << order;
	unsigned long page_idx = page - mem_map, buddy_idx;
	struct Page * buddy, * coalesced;
	while (order < 10) {
		// calculate the buddy index, by xor operation
		// if 1 << order bit was previously zero, buddy 
		// index is equal to page_idx + size, conversely,
		// if the bit was previously one, buddy index is
		// equal to page_idx - size
		buddy_idx = page_idx ^ size;
		buddy = &mem_map[buddy_idx];
		if (!page_is_buddy(buddy, order))
			break;
		LIST_REMOVE(buddy, lru);
		buddy->property = 0;
		page_idx &= buddy_idx;
		order ++;
	}

	coalesced = &mem_map[page_idx];
	coalesced->property = order;
	LIST_INSERT_HEAD(&(free_area[order].free_list), coalesced, lru);
}
