#ifndef _HYVEMIND_X64_ASM_SETUP_H
#define _HYVEMIND_X64_ASM_SETUP_H

#include "mm_types.h"

extern void arch_setup_bsp(void);
extern virt_addr_t arch_replace_stack(const unsigned int page_nr);
void arch_bringup_aps(void);

#endif /* _HYVEMIND_X64_ASM_SETUP_H */

