#ifndef _MEMLAYOUT_H_
#define _MEMLAYOUT_H_
#define KERNTOP  0x80000000  // top of kernel
// Virtual page table.  Entry PDX[VPT] in the PD contains a pointer to
// the page directory itself, thereby turning the PD into a page table,
// which maps all the PTEs containing the page mappings for the entire
// virtual address space into that 4 Meg region starting at VPT.
#define VPT      0x7fc00000  // virtual page table 
#define UVPT     0x7f800000  // virtual page table for user
#define KSTACKTOP 0xfeb00000  // kernel stack top
#endif
