#include "asm/apic.h"
#include "asm/current.h"
#include "asm/vmm.h"

#define INIT_CPU()                      \
    (struct cpu) {                      \
        .id = INVALID_CPUID,            \
        .lapic_id = INVALID_LAPIC_ID,   \
        .bsp = false,                   \
        .state = CPU_NOT_INIT,          \
        .vmxon_region_ptr = 0,          \
    }

DEFINE_PER_CPU_VAL(struct cpu, cpu, INIT_CPU());

static cpuid_t cpuid_counter = 0;

struct cpuid_lapic_id_pair {
    cpuid_t cpuid;
    lapicid_t lapic_id;
};

static struct cpuid_lapic_id_pair cpuid_lapic_id_pairs[MAX_CPUS] = {
    [0 ... MAX_CPUS-1] = { INVALID_CPUID, INVALID_LAPIC_ID }
};
static lapicid_t cpuid_to_lapic_id_map[MAX_CPUS] = {
    [0 ... MAX_CPUS-1] = INVALID_LAPIC_ID
};

lapicid_t
cpuid_to_lapic_id(const cpuid_t cpuid)
{
    return cpuid_to_lapic_id_map[cpuid];
}

static cpuid_t
__get_cpuid_for_lapic_id(const lapicid_t lapic_id, const bool create_if_missing)
{
    for (int i = 0; i < MAX_CPUS; i++) {
        struct cpuid_lapic_id_pair *entry = &cpuid_lapic_id_pairs[i];

        if (entry->lapic_id == lapic_id)
            return entry->cpuid;

        if (entry->lapic_id == INVALID_LAPIC_ID) {
            if (!create_if_missing)
                break;

            entry->lapic_id = lapic_id;
            entry->cpuid = cpuid_counter++;
            cpuid_to_lapic_id_map[entry->cpuid] = lapic_id;
            return entry->cpuid;
        }
    }

    return INVALID_CPUID;
}

/**
 * This function is supposed to only be called by the BSP during CPU topology
 * mapping to assign a cpuid to every cpu -> We don't need any synchronization
 * for the cpuid_counter.
 */
cpuid_t
get_cpuid_for_lapic_id(const lapicid_t lapic_id)
{
    return __get_cpuid_for_lapic_id(lapic_id, true);
}

cpuid_t
lapic_id_to_cpuid(const lapicid_t lapic_id)
{
    return __get_cpuid_for_lapic_id(lapic_id, false);
}

cpuid_t
get_current_cpuid(void)
{
    const lapicid_t lapic_id = read_lapic_id();
    return lapic_id_to_cpuid(lapic_id);
}

