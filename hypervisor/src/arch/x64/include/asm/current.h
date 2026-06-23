/**
 * Separately defined from e.g. the logical processor struct etc. to avoid
 * cyclic dependencies with 'per-cpu.h'.
 */
#ifndef _HYVEMIND_X64_ASM_CURRENT_H
#define _HYVEMIND_X64_ASM_CURRENT_H

#include "asm/arch_types.h"

cpuid_t get_current_cpuid(void);
void do_idle_loop(void);
/* Same as do_idle_loop, but first replaces the current stack */
void startup_do_idle_loop(void);

#endif /* _HYVEMIND_X64_ASM_CURRENT_H */

