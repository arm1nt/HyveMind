#ifndef _HYVEMIND_CPU_TOPOLOGY_H
#define _HYVEMIND_CPU_TOPOLOGY_H

#include "asm/arch_types.h"
#include "limine/limine.h"

#define CPUMASK_LEN 32
typedef uint32_t cpumask_t;

/* Mask of all present CPUs */
extern cpumask_t cpumask_present;
/* Mask of all online CPUs */
extern cpumask_t cpumask;

static inline cpuid_t
get_next_cpuid_in_mask(const cpumask_t mask, const cpuid_t offset)
{
    uint32_t idx = 0;
    while ((offset + idx) < CPUMASK_LEN) {
        if (mask & (1 << (offset + idx))) {
            return offset + idx;
        }
        idx++;
    }

    return INVALID_CPUID;
}

#define for_each_cpuid_in_mask(mask, id)        \
    for (                                       \
        id = get_next_cpuid_in_mask(mask, 0);   \
        id != INVALID_CPUID;                    \
        id = get_next_cpuid_in_mask(mask, id+1) \
    )
#define for_each_cpuid_in_cpumask(id) for_each_cpuid_in_mask(cpumask, id)
#define for_each_cpuid_in_cpumask_present(id) for_each_cpuid_in_mask(cpumask_present, id)

void map_cpu_topology(const struct limine_mp_response *mp_info);

#endif /* _HYVEMIND_CPU_TOPOLOGY_H */

