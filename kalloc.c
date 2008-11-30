// Physical memory allocator, intended to allocate
// memory for user processes. Allocates in 4096-byte "pages".
// Free list is kept sorted and combines adjacent pages into
// long runs, to make it easier to allocate big segments.
// One reason the page size is 4k is that the x86 segment size
// granularity is 4k.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "pmap.h"
#include "spinlock.h"
#include "assert.h"

struct spinlock kalloc_lock;

struct run {
  struct run *next;
  int len; // bytes
};
struct run *freelist;
uint npages;  // number of available pages
uint dbg = 0;
char * start;  // start address after kernel
struct e820map * e820_memmap;

void
init_pages_list(paddr_t start_addr, uint len, uint32_t flags)
{
  paddr_t addr;
  // page_frame macro does not work for addresses at the end of 4G
  if (start_addr >= 0xfec00000)
    return;
  assert(PGOFF(start_addr) == 0, "error in init_pages_list");
  for (addr = start_addr; addr < start_addr + len; addr += PAGE) {
//    cprintf("page descriptor addreee, %x, PPN %x\n", (uint)(page_frame(addr)),PPN(addr));
    page_frame(addr)->flags = flags;
  }
}

void
init_phypages(void)
{
  int i;
  uint len;
  paddr_t base;
  npages = 0;
  e820_memmap = (struct e820map *)(0x8000);

  for (i = 0; i < e820_memmap->nr_map; i ++) {
  //  if (e820_memmap->map[i].type == E820_ARM)
      npages += e820_memmap->map[i].size / PAGE;
  }
  
  cprintf("total available memory pages : %x\n", npages);

  pages = (struct Page *)start;
  memset(pages, 0, sizeof(struct Page) * npages);
  start += ROUNDUP(sizeof(struct Page) * npages, PAGE);
  cprintf("start %x\n",(uint)start);

  for (i = 0; i < e820_memmap->nr_map; i++) {
    base = (paddr_t)e820_memmap->map[i].addr;
    base = ROUNDUP(base, PAGE);
    len = (uint)e820_memmap->map[i].size;

    switch (e820_memmap->map[i].type) {
      case E820_ARM :
        if (base + len < (uint) start) {
          init_pages_list(base, len, PG_reserved);
          cprintf("reserved for kernel %x, size %x\n", base, len);
        }
        else {
          if (base < (uint)start) {
            init_pages_list(base, (uint)start - base, PG_reserved);
            cprintf("reserved for kernel %x, size %x\n", base, (uint)start - base);
            init_pages_list((paddr_t)start, len + base - (uint)start, 0);
            cprintf("free memory %x, size %x\n", (paddr_t)start, len + base - (uint)start);
          }
          else {
            init_pages_list(base, len, 0);
            cprintf("free memory %x, size %x\n", base, len);
          }
        }
        break;
      case E820_ARR :
        init_pages_list(base, len, PG_reserved);
        cprintf("reserved memmory %x, size %x\n", base, len);
        break;
      default:
        break;
    }
  }
}

// Initialize free list of physical pages.
// This code cheats by just considering one megabyte of
// pages after _end.  Real systems would determine the
// amount of memory available in the system and use it all.
void
kinit(void)
{
  extern int end;
  uint mem;
  char *baseaddr, *endaddr;
  int i;

  initlock(&kalloc_lock, "kalloc");
  start = (char*) &end;
  start = (char*) (((uint)start + PAGE) & ~(PAGE-1));
//  mem = 256; // assume computer has 256 pages of RAM
//  cprintf("mem = %d\n", mem * PAGE);
  init_phypages();

  for (i = 0; i < e820_memmap->nr_map; i ++) {
	  if (e820_memmap->map[i].type == E820_ARM) {
		  mem = e820_memmap->map[i].size;
		  baseaddr = (char *)(uint)e820_memmap->map[i].addr;

		  // Make sure the block of memory starts from page boundary
		  endaddr = ROUNDDOWN(baseaddr + mem, PAGE);
		  baseaddr = ROUNDUP(baseaddr, PAGE);
		  mem = (uint)(endaddr - baseaddr);

		  // do not use base memory (reserved for special use)
		  if ((uint)baseaddr < 0x100000)
			  continue;
		  cprintf("range : addr = %x, size = %x\n",(uint)baseaddr, mem);

		  // insert free memory
		  // decide whether kernel is inside the range
		  if (start >= baseaddr && start < baseaddr + mem) {
			  cprintf("free mem: %x, %x\n", start,baseaddr + mem - start);
			  kfree(start, baseaddr + mem - start);
		  }
		  else {
			  cprintf("free mem: %x, %x\n", baseaddr,mem);
			  kfree(baseaddr, mem);
		  }
	  }
  }
  cprintf("free_list %x\n",(uint)freelist);
  i386_vm_init();
//  kfree(start, mem * PAGE);
}

// Free the len bytes of memory pointed at by v,
// which normally should have been returned by a
// call to kalloc(len).  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v, int len)
{
  struct run *r, *rend, **rp, *p, *pend;

  if(len <= 0 || len % PAGE)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, len);

  acquire(&kalloc_lock);
  p = (struct run*)v;
  pend = (struct run*)(v + len);
  for(rp=&freelist; (r=*rp) != 0 && r <= pend; rp=&r->next){
    rend = (struct run*)((char*)r + r->len);
    if(r <= p && p < rend)
      panic("freeing free page");
    if(pend == r){  // p next to r: replace r with p
      p->len = len + r->len;
      p->next = r->next;
      *rp = p;
      goto out;
    }
    if(rend == p){  // r next to p: replace p with r
      r->len += len;
      if(r->next && r->next == pend){  // r now next to r->next?
        r->len += r->next->len;
        r->next = r->next->next;
      }
      goto out;
    }
  }
  // Insert p before r in list.
  p->len = len;
  p->next = r;
  *rp = p;

 out:
  release(&kalloc_lock);
}

// Allocate n bytes of physical memory.
// Returns a kernel-segment pointer.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(int n)
{
  char *p;
  struct run *r, **rp;

  if(n % PAGE || n <= 0)
    panic("kalloc");

  acquire(&kalloc_lock);
  for(rp=&freelist; (r=*rp) != 0; rp=&r->next){
    if(r->len == n){
      *rp = r->next;
      release(&kalloc_lock);
      return (char*)r;
    }
    if(r->len > n){
      r->len -= n;
      p = (char*)r + r->len;
      release(&kalloc_lock);
      return p;
    }
  }
  release(&kalloc_lock);

  cprintf("kalloc: out of memory\n");
  return 0;
}
