#ifndef _HYVEMIND_X64_ASM_APIC_H
#define _HYVEMIND_X64_ASM_APIC_H

#include "cpufeatures.h"
#include "processor.h"
#include "mm_types.h"
#include "asm/arch_types.h"

#define LAPIC_ID_REGISTER_OFFSET    0x20
#define LAPIC_ID_REGISTER_ID_OFFSET 24

static inline unsigned int
get_max_apic_addr(void)
{
    const unsigned int default_max_apic_addr = 36;

    if (!cpuid_leaf_in_range(CPUID_MAX_APIC_ADDR_LEAF)) {
        return default_max_apic_addr;
    }

    const cpuid_result_t res = cpuid_raw(CPUID_MAX_APIC_ADDR_LEAF, NO_SUBLEAF_INDEX);
    /* eax[7:0] contains the max apic addr width */
    return (res.eax) & (0xFF);
}

static inline phys_addr_t
get_lapic_base(void)
{
    const uint64_t mask = (U64(1) << get_max_apic_addr()) - 1;
    const uint64_t apic_base = (read_msr(MSRX64_IA32_APIC_BASE) >> 12) << 12;
    return apic_base & mask;
}

static inline lapicid_t
read_xapic_lapic_id(void)
{
    const phys_addr_t lapic_base = get_lapic_base();
    const uint32_t id_register = *((uint32_t *)(lapic_base + LAPIC_ID_REGISTER_OFFSET));
    return id_register >> LAPIC_ID_REGISTER_ID_OFFSET;
}

static inline lapicid_t
read_x2apic_lapic_id(void)
{
    return (lapicid_t) read_msr(MSR_X2APIC_LAPIC_ID_REGISTER);
}

static inline bool
in_x2apic_mode(void)
{
    return IS_SET(read_msr(MSRX64_IA32_APIC_BASE), MSR_APIC_BASE_EXTD);
}

static inline lapicid_t
read_lapic_id(void)
{
    if (in_x2apic_mode()) {
        return read_x2apic_lapic_id();
    } else {
        return read_xapic_lapic_id();
    }
}

#endif /* _HYVEMIND_X64_ASM_APIC_H */

