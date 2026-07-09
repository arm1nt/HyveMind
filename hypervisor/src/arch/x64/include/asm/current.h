/**
 * Separately defined from e.g. the logical processor struct etc. to avoid
 * cyclic dependencies with 'per-cpu.h'.
 */
#ifndef _HYVEMIND_X64_ASM_CURRENT_H
#define _HYVEMIND_X64_ASM_CURRENT_H

#include "asm/arch_types.h"

cpuid_t get_current_cpuid(void);

#endif /* _HYVEMIND_X64_ASM_CURRENT_H */

