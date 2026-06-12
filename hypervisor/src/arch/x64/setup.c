#include "fatal.h"
#include "asm/mm.h"
#include "mm_types.h"
#include "per-cpu.h"
#include "string.h"
#include "asm/apic.h"
#include "asm/pgtables.h"
#include "asm/setup.h"

DEFINE_PER_CPU_VAL(uint32_t, cpuid_range_base, 0x00);
DEFINE_PER_CPU(uint32_t, cpuid_max_leaf);
DEFINE_PER_CPU_VAL(uint32_t, cpuid_extended_range_base, 0x80000000);
DEFINE_PER_CPU(uint32_t, cpuid_max_extended_leaf);

static bool
cpuid_supported(void)
{
    uint64_t temp_rflags;
    const uint64_t orig_rflags = read_rflags();

    write_rflags(SET_BIT(orig_rflags, EFLAGS_ID));
    temp_rflags = read_rflags();
    if (IS_CLEAR(temp_rflags, EFLAGS_ID)) {
        return false;
    }

    write_rflags(CLEAR_BIT(temp_rflags, EFLAGS_ID));
    temp_rflags = read_rflags();
    if (IS_SET(temp_rflags, EFLAGS_ID)) {
        return false;
    }

    write_rflags(orig_rflags);

    return true;
}

static inline void
query_max_leaf_values(void)
{
    cpuid_result_t result;

    uint32_t *cpuid_max_leaf = percpu_ptr(cpuid_max_leaf);
    uint32_t *cpuid_max_extended_leaf = percpu_ptr(cpuid_max_extended_leaf);

    result = cpuid_raw(CPUID_BASE_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    *cpuid_max_leaf = result.eax;

    result = cpuid_raw(CPUID_EXTENDED_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    *cpuid_max_extended_leaf = result.eax;
}

static inline bool
is_genuine_intel(void)
{
    const cpuid_result_t result = cpuid_raw(CPUID_BRAND_STRING_LEAF, NO_SUBLEAF_INDEX);

    if (memcmp("Genu", (char *) &result.ebx, 4) != 0
        || memcmp("ineI", (char *) &result.edx, 4) != 0
        || memcmp("ntel", (char *) &result.ecx, 4) != 0) {
        return false;
    }

    return true;
}

static inline bool
all_cpu_features_supported(void)
{
    cpuid_result_t result;

    if (cpuid(CPUID_CPU_FEATURES_LEAF, NO_SUBLEAF_INDEX, &result) != 0) {
        printf("Unable to query CPU feature information as leaf \"0x01\" is not supported");
        return false;
    }

    if (IS_CLEAR(result.ecx, CPUID_VMX)) {
        printf("The processor does not support the virtual machine extensions!");
        return false;
    }

    if (IS_CLEAR(result.edx, CPUID_MSR)) {
        printf("The processor does not support the 'rdmsr' and 'wrmsr' instructions");
        return false;
    }

    return true;
}

static inline bool
check_cpu(void)
{
    if (!cpuid_supported()) {
        printf("Procesor does not support CPUID");
        return false;
    }

    query_max_leaf_values();

    if (!is_genuine_intel()) {
        printf("Not a genuine Intel CPU");
        return false;
    }

    if (!all_cpu_features_supported()) {
        printf("Not all required CPU features are supported");
        return false;
    }

    return true;
}

static bool
create_addr_space(void)
{
    struct cr3 new_addr_space;
    memset(&new_addr_space, 0, sizeof(new_addr_space));

    const phys_addr_t lapic_base = get_lapic_base();
    if (raw_identity_map_mmio_page(&new_addr_space, lapic_base) != 0) {
        printf("Failed to identity map the APIC page");
        return false;
    }

    struct mapping_info info = {
        .addr_space = &new_addr_space,
        .curr_offset = EARLY_DIRECT_MAPPING_OFFSET,
        .target_offset = HYPERVISOR_DIRECT_MAPPING_OFFSET,
        .ps_flags = PGTABLE_RW,
        .nops_flags = PGTABLE_RW
    };

    const boot_mem_info_t *bm_info = (boot_mem_info_t *) boot_mem_info_scratch;
    const hyv_memmap_t *mem_map = (hyv_memmap_t *) __phys_to_virt(
            bm_info->mem_map,
            EARLY_DIRECT_MAPPING_OFFSET
    );

    if (setup_direct_mapping_from_memmap(&info, mem_map) != 0) {
        printf("Failed to create direct mapping of system memory into BSP addr space");
        return false;
    }

    const phys_addr_t paddr_hyv_region_start = bm_info->hypervisor_region.paddr_start;
    const virt_addr_t vaddr_hyv_region_start = bm_info->hypervisor_region.vaddr_start;
    const uint64_t hyv_region_length = bm_info->hypervisor_region.length;

    info.target_offset = vaddr_hyv_region_start - paddr_hyv_region_start;

    if (directly_map_phys_range(&info, paddr_hyv_region_start, paddr_hyv_region_start + hyv_region_length - 1) != 0) {
        printf("Failed to create direct mapping of hypervisor code in BSP addr space");
        return false;
    }

    write_cr3(*(const uint64_t *) &new_addr_space);

    return true;
}

void
__arch_setup_bsp(void)
{
    /* We map this into the bootloader provided page tables for early bootstraping */
    const phys_addr_t lapic_base = get_lapic_base();
    if (identity_map_mmio_page(lapic_base) != 0) {
        die_reason("Unable to identity map the APIC page");
    }

    if (check_cpu() == false) {
        die_reason("Logical processor cannot support hyvemind operation");
    }

    if (early_mm_init() != 0) {
        die_reason("Unable to identity basic cpu mm information");
    }

    if (create_addr_space() == false) {
        die_reason("Failed to the address space of the BSP");
    }
}

static void
arch_setup_ap(void)
{
    return;
}


void
arch_bringup_aps(void)
{
    return;
}

