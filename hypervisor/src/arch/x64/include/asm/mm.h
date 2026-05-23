#ifndef _HYVEMIND_X64_ASM_MM_H
#define _HYVEMIND_X64_ASM_MM_H

#include "limine/limine.h"
#include <stdint.h>

extern uint64_t max_phys_addr;
#define MAX_PHYS_ADDR max_phys_addr;

int init_mm(const struct limine_memmap_response *mem_map);

#endif /* _HYVEMIND_X64_ASM_MM_H */

