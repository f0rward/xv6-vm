#ifndef _ASSERT_H_
#define _ASSERT_H_

#define assert(x, msg)		\
	do { if (!(x)) panic(msg); } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#endif
