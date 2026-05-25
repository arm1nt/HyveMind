#include "printf.h"
#include "processor.h"
#include "string.h"
#include "asm/mm.h"
#include "asm/direct_mapping.h"
#include "asm/paging.h"
#include "asm/pgtable_types.h"

bool supports_1gb_pages = false;
uint64_t max_phys_addr;
uint64_t early_direct_mapping_offset;

static inline int
get_max_phys_addr(void)
{
    cpuid_result_t result;
    if (cpuid(CPUID_MAX_PHYS_ADDR_LEAF, NO_SUBLEAF_INDEX, &result) != 0) {
        return -1;
    }

    /* Max phys addr size is stored in bits 0..7 of result.eax */
    const uint32_t bits = result.eax & ((U32(1) << 8) - 1);
    max_phys_addr = (U64(1) << bits) - 1;

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

static int
_early_setup_direct_mapping(
        const struct limine_memmap_response *mem_map,
        const struct limine_executable_address_response *exec_addr_info
)
{
    const struct mapping_info mapping_info = {
        .curr_offset = EARLY_DIRECT_MAPPING_OFFSET,
        .target_offset = HYPERVISOR_DIRECT_MAPPING_OFFSET
    };

    struct cr3 new_addr_space;
    memset(&new_addr_space, 0, sizeof(struct cr3));

    if (early_setup_direct_mapping(&new_addr_space, mem_map, &mapping_info) != 0) {
        return -1;
    }

    /**
     * The bootloader maps the executable at 0xffffffff80000000 as well
     * so its included in the direct mapping and there is an extra mapping too
     * -> We have to map both
     */

    /* todo: do properly */
    const struct mapping_info scnd = {
        .curr_offset = EARLY_DIRECT_MAPPING_OFFSET,
        .target_offset = exec_addr_info->virtual_base - exec_addr_info->physical_base
    };
    directly_map_range(
            &new_addr_space,
            &scnd,
            exec_addr_info->physical_base,
            exec_addr_info->physical_base + (0xa000-1));

    const  uint64_t *cr3_flattened = (uint64_t *) &new_addr_space;
    write_cr3(*cr3_flattened);

    /* need to change the return instruction pointer when the offset of the
     * direct mapping is different from the one we create */

    return 0;
}

int
init_mm(const struct limine_memmap_response *mem_map, const struct limine_executable_address_response *exec_addr_info)
{
    printf("Starting X64 specific memory management initialization...");

    if (get_max_phys_addr() != 0) {
        printf("Unable to query the max physical addr size supported");
        return -1;
    }

    check_supported_page_sizes();

    if (_early_setup_direct_mapping(mem_map, exec_addr_info) != 0) {
        printf("Failed to install direct mapping");
        return -1;
    }

    return 0;
}

