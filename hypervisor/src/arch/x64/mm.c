#include "printf.h"
#include "per-cpu.h"
#include "asm/cpufeatures.h"
#include "asm/processor.h"
#include "asm/mm.h"

bool supports_1gb_pages = false;

DEFINE_PER_CPU_VAL(uint64_t, max_phys_addr,  0xFFFFFFFFFFFFF);
/* Keep until all usages in the page table logic is removed */
uint64_t max_phys_addr = 0xFFFFFFFFFFFFF;
uint64_t early_direct_mapping_offset;

static inline int
get_max_phys_addr(void)
{
    uint64_t *max_phys_addr = percpu_ptr(max_phys_addr);

    cpuid_result_t result;
    if (cpuid(CPUID_MAX_PHYS_ADDR_LEAF, NO_SUBLEAF_INDEX, &result) != 0) {
        return -1;
    }

    /* Max phys addr size is stored in bits 0..7 of result.eax */
    const uint32_t bits = result.eax & ((U32(1) << 8) - 1);
    *max_phys_addr = (U64(1) << bits) - 1;

    return 0;
}

static inline void
check_supported_page_sizes(void)
{
    cpuid_result_t result;
    if (cpuid(CPUID_PAGE_1GB_LEAF, NO_SUBLEAF_INDEX, &result) != 0) {
        /* If feature-info leaf is not valid, assume 1GB pages are not supported */
        return;
    }

    if (IS_SET(result.edx, CPUID_PAGE_1GB)) {
        // TOOD: re-enable later again
        //supports_1gb_pages = true;
    }
}

int
early_mm_init(void)
{
    if (get_max_phys_addr() != 0) {
        pr_error("Unable to determine the processor's max supported physical addr width");
        return -1;
    }

    check_supported_page_sizes();
    return 0;
}

