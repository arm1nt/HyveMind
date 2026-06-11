#ifndef _HYVEMIND_X64_ASM_APIC_H
#define _HYVEMIND_X64_ASM_APIC_H

#include "cpufeatures.h"
#include "processor.h"
#include "mm_types.h"

typedef uint32_t lapicid_t;

#define APIC_BASE_REGISTER          0xFEE00000
#define LAPIC_ID_REGISTER_OFFSET    0x20

static inline phys_addr_t
get_lapic_base(void)
{
    return read_msr(MSRX64_IA32_APIC_BASE) & U64(0xFFFFF000);
}

static inline lapicid_t
read_lapic_id(void)
{
    /* The local APIC's registers must be identity mapped! */
    const phys_addr_t lapic_base = get_lapic_base();
    const uint32_t lapic_id = *((uint32_t *)(lapic_base + LAPIC_ID_REGISTER_OFFSET));
    return lapic_id;
}

#endif /* _HYVEMIND_X64_ASM_APIC_H */

