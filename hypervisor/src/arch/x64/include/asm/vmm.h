#ifndef _HYVEMIND_X64_ASM_VMM_H
#define _HYVEMIND_X64_ASM_VMM_H

#include "types.h"
#include "mm_types.h"
#include "per-cpu.h"

enum logical_processor_state {
    PROCESSOR_NOT_INIT,
    /* If we were unable to setup the processor properly */
    PROCESSOR_UNAVAILABLE,
    /* If we don't want to use the certain logical processor */
    PROCESSOR_DISABLED,
    PROCESSOR_IDLE,
    PROCESSOR_BUSY
};
typedef enum logical_processor_state processor_state_t;

struct logical_processor {
    cpuid_t processor_id;
    lapicid_t lapic_id;
    bool is_bsp;
    processor_state_t state;

    uint64_t raw_cr3;
    void *stack_ptr;

    phys_addr_t vmxon_region_ptr;
    bool vmx_operation_active;
};
typedef struct logical_processor logical_processor_t;

#define INIT_LOGICAL_PROCESSOR() ((logical_processor_t) {   \
    .processor_id = INVALID_PROCESSOR_ID,                   \
    .lapic_id = INVALID_LAPIC_ID,                           \
    .is_bsp = false,                                        \
    .state = PROCESSOR_NOT_INIT,                            \
    .vmxon_region_ptr = 0,                                  \
    .vmx_operation_active = false                           \
    })

DECLARE_PER_CPU(logical_processor_t, logical_processor);

logical_processor_t get_logical_processor_by_id(const cpuid_t id);
logical_processor_t get_current_logical_processor(void);

#endif /* _HYVEMIND_X64_ASM_VMM_H */

