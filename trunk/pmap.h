#ifndef _PMAP_H_
#define _PMAP_H_
#include "queue.h"
#include "types.h"
#include "mmu.h"
#include "atomic.h"
#define NULL ((void *) 0)

// define some constants for bios interrupt 15h AX = E820h
#define E820MAX    32  // number of entries in E820MAP
#define E820_ARM   1   // address Range Memory
#define E820_ARR   2   // address Range Reserved

struct e820map {
	int nr_map;
	struct {
		long long addr;
		long long size;
		long type;
	} map[E820MAX];
};

/* Physical pages descriptor, each Page describes a physical page*/
typedef LIST_HEAD(Page_list, Page) page_list_head_t;
typedef LIST_ENTRY(Page) page_list_entry_t;

struct Page {
	uint32_t flags;  // flags for page descriptors
	atomic_t mapcount;  // number of page table entries that refer to the page frame
	uint32_t property;  // when the page is free , this field is used by the buddy system
	uint32_t index;
	page_list_entry_t lru; /* free list link */
};

typedef struct Page page_t;

extern pde_t * boot_pgdir;
void init_phypages(void);
pte_t *get_pte(pde_t * pgdir, vaddr_t va, int create);
int insert_page(pde_t * pgdir, paddr_t pa, vaddr_t va, uint perm);
void i386_vm_init(void);
void enable_paging(void);
char * alloc_page();
#endif
