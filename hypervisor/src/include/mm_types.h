#ifndef _HYVEMIND_MM_TYPES_H
#define _HYVEMIND_MM_TYPES_H

#include "stdint.h"

/* Marker for verbosity */
#define __directly_mapped

typedef uint64_t virt_addr_t;
typedef uint64_t phys_addr_t;
typedef uint64_t pfn_t;

#define __vaddr(x) ((virt_addr_t) (x))

#endif /* _HYVEMIND_MM_TYPES_H */

