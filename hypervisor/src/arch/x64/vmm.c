#include "asm/apic.h"
#include "asm/current.h"
#include "asm/vmm.h"

DEFINE_PER_CPU_VAL(logical_processor_t, logical_processor, INIT_LOGICAL_PROCESSOR());

cpuid_t
get_current_cpuid(void)
{
    return (cpuid_t) read_lapic_id();
}

