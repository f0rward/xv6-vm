#include "pmap.h"
#include "memlayout.h"
#include "param.h"
#include "x86.h"
#include "defs.h"
#include "assert.h"
#include "errorno.h"
#include "spinlock.h"
#include "buddy.h"

spinlock_t phy_mem_lock;
struct Page * pages;    // Physical Page descriptor array
paddr_t boot_cr3;    // physical address of boot time page directory
pde_t * boot_pgdir;    // virtual address of boot time page directory
struct Page * pages;    // all page descriptors 
//static uchar * boot_freemem = 0; // pointer to next byte of free mem

static void check_boot_pgdir();

static void
boot_map_segment(pde_t * pgdir, paddr_t pa, vaddr_t la, uint size, uint perm)
{
	int ret = 0;
	uint i = 0;
	assert(!(size & 0xfff), "size is not a multiple of PAGE\n");
	for (; i < size; i += PAGE) {
		if ((ret = insert_page(pgdir, pa + i, la + i, perm, 1)) < 0) {
			cprintf("error %d\n",ret);
			panic("error at boot map segment\n");
		}
	}
}

// Map the segment with physical address pa at linear addresss la
// 
// RETURNS
// 0 on success
// -E_MAP_EXIST, if there is already a page mapped at 'va'
// -E_NO_MEM, if the page table couldn't be allocated
// -E_NOT_AT_PGBOUND, if pa or va or size is not at page boundary
int
map_segment(pde_t * pgdir, paddr_t pa, vaddr_t la, uint size, uint perm)
{
  int ret = 0;
  uint i = 0;
  if (size & 0xfff)
    return -E_NOT_AT_PGBOUND;
  for (; i < size; i += PAGE) {
    if ((ret = insert_page(pgdir, pa + i, la + i, perm, 0)) < 0) {
      return ret;
    }
  }
  return 0;
}

int
do_unmap(pde_t * pgdir, vaddr_t va, uint size)
{
  uint i,ret;
  if (va & 0xfff || size & 0xfff)
    panic("do_unmap invalid va or size");
  for (i = 0; i < size; i += PAGE) {
    if ((ret = remove_page(pgdir, va)) < 0)
      return ret;
    va += PAGE;
  }
  return 0;
}

int
unmap_userspace(pde_t * pgdir)
{
  uint index,pteidx,ret;
  pte_t * pte;
  if (!pgdir)
    return 0;
  dbmsg("unmap user space\n");
  for (index = PDX(KERNTOP); index < PDX(0xfec00000); index ++) {
    if (pgdir[index] & PTE_P) {
      pte = (pte_t *)PTE_ADDR(pgdir[index]);
      for (pteidx = 0; pteidx < PTENTRY; pteidx ++) {
        if (pte[pteidx] & PTE_P) {
          ret = remove_pte(pgdir, &pte[pteidx]);
          if (ret < 0)
            return ret;
        }
      }
      remove_pte(pgdir, pte);
    }
  }
  return 0;
}

// Get a single page frame
char *
alloc_page()
{
  return kalloc(PAGE);
}

// Map the physical page at virtual address 'va'
// The permissions of the page table entry should
// be set to 'perm | PTE_P'
//
// RETURNS:
// 0 on success
// -E_NOT_AT_PGBOUND, if pa or va is not at page boundary
// -E_NO_MEM, if the page table couldn't be allocated
// -E_MAP_EXIST, if there is already a page mapped at 'va'
int
insert_page(pde_t * pgdir, paddr_t pa, vaddr_t va, uint perm, uint kmap)
{
	if ((pa & 0xfff) || (va & 0xfff))
		return -E_NOT_AT_PGBOUND;

	acquire(&phy_mem_lock);
	pte_t * pte = get_pte(pgdir, va, 1);

	if (pte == NULL) {
		release(&phy_mem_lock);
		return -E_NO_MEM;
	}

	if (*pte & PTE_P) {
		release(&phy_mem_lock);
		return -E_MAP_EXIST;
	}
	*pte = PTE_ADDR(pa) | PTE_P | perm;

        if (!kmap) {
          IncPageCount(page_frame(pa));
        }
        
	release(&phy_mem_lock);

	return 0;
}

// Remove the mapping at va
// If the page is not mapped any more , free the page
// RETURNS:
// 0 on success
// -E_ALREADY_FREE if va is already free
//
int
remove_page(pde_t * pgdir, vaddr_t va)
{
  pte_t * pte;
  if (va & 0xfff)
    return -E_NOT_AT_PGBOUND;
  pte = get_pte(pgdir, va, 0);
  return remove_pte(pgdir, pte);
}

// Remove the mapping at pte
// RETURNS:
// 0 on success
// -E_ALREADY_FREE if pte is already free
// 
int
remove_pte(pde_t * pgdir, pte_t * pte)
{ 
  struct Page * p;
  acquire(&phy_mem_lock);
  if (pte == NULL)
    return -E_ALREADY_FREE;

  if (*pte & PTE_P) {
    p = page_frame(PTE_ADDR(*pte));
    DecPageCount(p);
    if (!PageReserved(p) && !IsPageMapped(p)) {
      dbmsg("removing mapping at pages %x\n", p - pages);
      __free_pages(p, 1);
    }
    *pte = 0;
  }
  else {
    release(&phy_mem_lock);
    return -E_ALREADY_FREE;
  }

  release(&phy_mem_lock);
  return 0;
}

// Enable paging
// Load cr3 and set PE & PG bit in cr0 register
void
enable_paging(void)
{
	uint cr0;
	// install page talbe
	lcr3(boot_cr3);

	// turn on paging
	cr0 = rcr0();
	cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
	cr0 &= ~(CR0_TS | CR0_EM);
	lcr0(cr0);
}

// Setup a two-level page table:
// 	boot_pgdir is the linear address of the root
// 	boot_cr3 is the physical address of the root
// After that turn on paging.
void
i386_vm_init(void)
{
	pde_t * pgdir;
	int i;
	// init physical memory allocation and deallocation spin lock
	initlock(&phy_mem_lock, "phy_mem");

	// create initial page directory , no need to acquire spin lock because
	// no other processors are running
	pgdir = (pde_t *)alloc_page();
	memset(pgdir, 0, PAGE);
	boot_pgdir = pgdir;
	boot_cr3 = (uint)pgdir;

	// turn the page directory into a page table so that all page table 
	// entries containing mappings for the entire space will be mapped 
	// into the 4 Meg region at VPT
	pgdir[PDX(VPT)] = boot_cr3 | PTE_W | PTE_P;
	pgdir[PDX(UVPT)] = boot_cr3 | PTE_U | PTE_P;

	// map the range of memory from 0~0x100000 to itself
	boot_map_segment(pgdir, 0, 0, 0x100000, PTE_U | PTE_W);
	for (i = 0; i < e820_memmap->nr_map; i ++) {
		uint addr = (uint)e820_memmap->map[i].addr;
		uint size = (uint)e820_memmap->map[i].size;
		if (addr >= 0x100000) {
			cprintf("map : %x,%x with size %x\n",(addr >> 16),(addr & 0xffff),size);
			boot_map_segment(pgdir, addr, addr, size, PTE_U | PTE_W); 
		}
	}
	// hard code : map 0xfec00000
	boot_map_segment(pgdir, 0xfec00000, 0xfec00000, 0x1000000, PTE_U | PTE_W);

	// check the initial page directory has been set up correctly
	check_boot_pgdir();

	enable_paging();
	cprintf("Paging enabled!\n");
}

// Given 'pgdir', a pointer to a page directory, get_pte returns
// a pointer to the page table entry (PTE) for linear address 'va'.
// This requires walking the two-level page table structure.
//
// If the relevant page table doesn't exist in the page directory, then:
//    - If create == 0, get_pte returns NULL.
//    - Otherwise, get_pte tries to allocate a new page table
//      with alloc_page.  If this fails, pgdir_walk returns NULL.
//    - pgdir_walk sets pp_ref to 1 for the new page table.
//    - Finally, get_pte returns a pointer into the new page table.
//

pte_t *
get_pte(pde_t * pgdir, vaddr_t va, int create)
{
	pde_t * pde = &pgdir[PDX(va)];
	pte_t * pte = NULL;
	if (!(*pde & PTE_P)) {
		if (create) {
			// allocate a page
//                        cprintf("get pte addr %x, PDX %x, PDE %x, pgdir address %x\n", va, PDX(va), *pde, (uint)pgdir);
			pte = (pte_t *)alloc_page();
			if (pte == NULL) return NULL;
			assert(!((uint)pte & 0xfff), "assert : not in page boundary\n");

			// initialize
			memset(pte,0,PAGE);
			// set flags
			*pde = (uint)pte | PTE_P | PTE_W | PTE_U;
//                        cprintf("pde update %x, adddr %x\n", *pde, (uint)pde);
		}
		else {
			// no pde exists, return NULL
			return NULL;
		}
	}

	pte = &(((pte_t *)PTE_ADDR(*pde))[PTX(va)]);
	return pte;
}

paddr_t
check_va2pa(pde_t * pgdir, vaddr_t va)
{
	if (!(pgdir[PDX(va)] & PTE_P))
		return -1;
	pte_t * pte = (pte_t *)PTE_ADDR(pgdir[PDX(va)]);
	if (!(pte[PTX(va)] & PTE_P))
		return -2;
	return PTE_ADDR(pte[PTX(va)]) | PGOFF(va); 
}

static void
check_boot_pgdir()
{
	uint i = 0x100000, ret = 0;
	for (;i < 0x200000; i += PAGE / 2) {
		ret = check_va2pa(boot_pgdir, i);
		if (ret == -1) {
			cprintf("pde not exist %x\n",i);
			continue;
		}
		if (ret == -2) {
			cprintf("pte not exist %x\n",i);
			continue;
		}
		if (ret != i)
			cprintf("address translate error %x\n",i);
	}
}
