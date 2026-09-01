#ifndef _HYVEMIND_X64_ASM_APIC_NEW_H
#define _HYVEMIND_X64_ASM_APIC_NEW_H

#include "mm_types.h"
#include "asm/arch_types.h"
#include "asm/cpufeatures.h"
#include "asm/processor.h"

enum apic_error {
    APIC_SUCCESS,
    APIC_NOT_PRESENT,
    APIC_ERROR,
};

#define x2apic_mode_enabled() \
    (IS_SET(read_msr(MSRX64_IA32_APIC_BASE), MSR_APIC_BASE_EXTD))

static inline lapicid_t
read_lapic_id(void)
{
    lapicid_t id;
    cpuid_result_t res;

    if (x2apic_mode_enabled()) {
        res = cpuid_raw(CPUID_EXT_TOPOLOGY_LEAF, NO_SUBLEAF_INDEX);
        id = res.edx;
    } else {
        res = cpuid_raw(CPUID_CPU_FEATURES_LEAF, NO_SUBLEAF_INDEX);
        id = res.ebx >> 24;
    }

    return id;
}

int setup_bsp_apic(void);
int setup_ap_apic(void);
int apic_send_ipi(void);
void apic_signal_eoi(void);
phys_addr_t get_lapic_base(void);
uint32_t get_apic_version(void);

int apic_program_tsc_deadline_timer(const uint64_t tsc_deadline);

#endif /* _HYVEMIND_X64_ASM_APIC_NEW_H */

