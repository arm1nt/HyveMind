#ifndef _HYVEMIND_MM_TYPES_H
#define _HYVEMIND_MM_TYPES_H

#include "stdint.h"

/* Marker for verbosity */
#define __directly_mapped

/* Markers to help indicate who owns some data */
#define __owned
#define __borrowed

typedef uint64_t virt_addr_t;
typedef uint64_t phys_addr_t;
typedef uint64_t pfn_t;

/* guest physical address */
typedef uint64_t gpaddr;
/* guest virtual address */
typedef uint64_t gvaddr;

#define __vaddr(x) ((virt_addr_t) (x))

#endif /* _HYVEMIND_MM_TYPES_H */

