#ifndef _HYVEMIND_HYVSTDLIB_H
#define _HYVEMIND_HYVSTDLIB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* undef to prevent the predefined value from messing up our own macros */
#undef linux

#define MAX(x,y) (((x) < (y)) ? (y) : (x))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#define ROUND_UP(x, y) ((x + ((y) - 1)) / (y))

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

#define __TO_STR(x) #x
#define TO_STR(x) __TO_STR(x)

#define __CONCAT(t1, t2, delim) t1 ## delim ## t2
#define CONCAT(t1, t2)  __CONCAT(t1, t2, _)
#define CONCAT_RAW(t1, t2, delim) __CONCAT(t1, t2, delim)

#endif /* _HYVEMIND_HYVSTDLIB_H */

