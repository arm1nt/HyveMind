#ifndef _HYVEMIND_X64_ASM_SETUP_H
#define _HYVEMIND_X64_ASM_SETUP_H

#include "mm_types.h"
#include "limine/limine.h"

/* Size in number of pages */
#define DEFAULT_HYV_THREAD_STACK_SIZE   16

extern void arch_setup_bsp(void);
extern virt_addr_t arch_replace_stack(const unsigned int page_nr);

/**
 * @page_nr ... Size of the newly allocated stack region in pages
 * @frames_to_unwind ... How many stack frames of the current call stack
 *                      should be copied/included into the new stack.
 *                      The number includes the frame of the calling function,
 *                      i.e. fct(X, 1) means that the new stack will contain only
 *                      the frame of the calling function, so you e.g. cannot
 *                      return from the function anymore.
 */
extern virt_addr_t arch_rebase_current_stack(
        const unsigned int page_nr,
        const unsigned int frames_to_unwind
);

void arch_bringup_aps(void);
void arch_bringup_aps_limine(const struct limine_mp_response *mp_info);

#endif /* _HYVEMIND_X64_ASM_SETUP_H */

