#ifndef _HYVEMIND_X64_ASM_SETUP_H
#define _HYVEMIND_X64_ASM_SETUP_H

#include "mm_types.h"
#include "limine/limine.h"

/* Size in number of pages */
#define DEFAULT_HYV_THREAD_STACK_SIZE   8

extern void arch_setup_bsp(void);
extern virt_addr_t arch_replace_stack(const unsigned int page_nr);
void arch_bringup_aps(void);
void arch_bringup_aps_limine(const struct limine_mp_response *mp_info);

#endif /* _HYVEMIND_X64_ASM_SETUP_H */

