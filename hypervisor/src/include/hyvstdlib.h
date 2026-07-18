#ifndef _HYVEMIND_HYVSTDLIB_H
#define _HYVEMIND_HYVSTDLIB_H

#include <stdint.h>
#include <stdbool.h>

#define MAX(x,y) (((x) < (y)) ? (y) : (x))
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#define ROUND_UP(x, y) ((x + ((y) - 1)) / (y))

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

#endif /* _HYVEMIND_HYVSTDLIB_H */

