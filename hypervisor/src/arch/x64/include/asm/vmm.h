#ifndef _HYVEMIND_X64_ASM_VMM_H
#define _HYVEMIND_X64_ASM_VMM_H

#include "hyvstdlib.h"
#include "mm_types.h"
#include "per-cpu.h"

enum cpu_state {
    CPU_NOT_INIT,
    CPU_INIT,
    CPU_UNAVAILABLE,
};

struct cpu {
    cpuid_t id;
    lapicid_t lapic_id;
    bool bsp;
    enum cpu_state state;

    uint64_t raw_cr3;

    phys_addr_t vmxon_region_ptr;
};
typedef struct cpu cpu_t;

DECLARE_PER_CPU(struct cpu, cpu);

cpuid_t get_cpuid_for_lapic_id(const lapicid_t lapic_id);
cpuid_t lapic_id_to_cpuid(const lapicid_t lapic_id);

lapicid_t cpuid_to_lapic_id(const cpuid_t cpuid);

#define current_cpu ((cpu_t *) percpu_ptr(cpu))

#endif /* _HYVEMIND_X64_ASM_VMM_H */

