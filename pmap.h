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

// flags describing the status of a page frame
#define PG_reserved  1  // the page frame is reserved for kernel code or is unusable
#define PG_property  2  // the property field of the page descriptor stores meaningful data
#define PG_locked    4  // the page is locked
#define PG_dirty     8  // the page has been modified

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
	uint32_t mapcount;  // number of page table entries that refer to the page frame
	uint32_t property;  // when the page is free , this field is used by the buddy system
	uint32_t index;
	page_list_entry_t lru; /* free list link */
};

typedef struct Page page_t;

extern pde_t * boot_pgdir;
extern struct Page * pages;
extern struct e820map * e820_memmap;
// get the descriptor of the page frame for address addr
// notice that it does not work for address at the end of 4G space
#define page_frame(addr) (&pages[PPN(addr)])
#define page_addr(p) ((paddr_t)(p - pages) << PTXSHIFT)

pte_t *get_pte(pde_t * pgdir, vaddr_t va, int create);
int insert_page(pde_t * pgdir, paddr_t pa, vaddr_t va, uint perm, uint kmap);
void i386_vm_init(void);
void enable_paging(void);
char * alloc_page();
int map_segment(pde_t * pgdir, paddr_t pa, vaddr_t la, uint size, uint perm);
int remove_page(pde_t * pgdir, vaddr_t va);
int do_unmap(pde_t * pgdir, vaddr_t va, uint size);
int remove_pte(pde_t * pgdir, pte_t * pte);
int unmap_userspace(pde_t * pgdir);
paddr_t check_va2pa(pde_t * pgdir, vaddr_t va);

#define SET_PAGE_RESERVED(page) ((page)->flags |= PG_reserved)
#define CLEAR_PAGE_RESERVED(page) ((page)->flags &= (~PG_reserved))
#define PageReserved(page) ((page)->flags & PG_reserved)
#define SetPageProperty(page) ((page)->flags |= PG_property)
#define ClearPageProperty(page) ((page)->flags &= (~PG_property))
#define PageProperty(page) ((page)->flags & PG_property)
#define IncPageCount(page) ((page)->mapcount ++)
#define DecPageCount(page) ((page)->mapcount --)
#define IsPageMapped(page)  ((page)->mapcount)
#endif
