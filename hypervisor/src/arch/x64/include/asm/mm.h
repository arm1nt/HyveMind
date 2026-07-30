#ifndef _HYVEMIND_X64_ASM_MM_H
#define _HYVEMIND_X64_ASM_MM_H

#include <stdint.h>
#include "per-cpu.h"

DECLARE_PER_CPU(uint64_t, max_phys_addr);
#define MAX_PHYS_ADDR percpu_val(max_phys_addr)

extern uint64_t max_phys_addr;

int early_mm_init(void);

#endif /* _HYVEMIND_X64_ASM_MM_H */

