#ifndef _TYPES_H_
#define _TYPES_H_
typedef unsigned int   uint;
typedef unsigned int   uint32_t;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
typedef uint pte_t;
typedef uint vaddr_t;
typedef uint paddr_t;

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a,n)			\
({					\
 	uint __a = (uint)(a);		\
 	(typeof(a))(__a - __a % (n));	\
 })

// Round up to the nearest multiple of n
#define ROUNDUP(a, n)						\
({								\
	uint __n = (uint) (n);					\
	(typeof(a)) (ROUNDDOWN((uint) (a) + __n - 1, __n));	\
})
#endif
