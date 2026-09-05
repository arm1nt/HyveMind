#ifndef _HYVEMIND_HYVSTDLIB_H
#define _HYVEMIND_HYVSTDLIB_H

#include <stdint.h>
#include <stdbool.h>

#define MAX(x,y) (((x) < (y)) ? (y) : (x))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#define ROUND_UP(x, y) ((x + ((y) - 1)) / (y))

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

#define TO_STR(x) #x

#define __CONCAT(t1, t2, separator) t1 ## separator ## t2
#define CONCAT(t1, t2)  __CONCAT(t1, t2, _)

#endif /* _HYVEMIND_HYVSTDLIB_H */

