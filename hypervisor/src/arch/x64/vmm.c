#include "asm/apic.h"
#include "asm/current.h"

/* For simplicity, we'll do the mapping like this for now */
static cpuid_t lapic_id_to_cpu_id[HYVEMIND_MAX_NR_CPUS] = {
    [0 ... HYVEMIND_MAX_NR_CPUS-1] = 0
};

cpuid_t
get_current_cpuid(void)
{
    const lapicid_t lapic_id = read_lapic_id();
    return lapic_id_to_cpu_id[lapic_id];
}

