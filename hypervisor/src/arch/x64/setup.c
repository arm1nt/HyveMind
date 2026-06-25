#include "fatal.h"
#include "mm_types.h"
#include "per-cpu.h"
#include "sched.h"
#include "string.h"
#include "asm/apic.h"
#include "asm/gdt_idt.h"
#include "asm/mm.h"
#include "asm/pgtables.h"
#include "asm/setup.h"
#include "asm/vmm.h"

DEFINE_PER_CPU_VAL(uint32_t, cpuid_range_base, CPUID_RANGE_BASE_VAL);
DEFINE_PER_CPU(uint32_t, cpuid_max_leaf);
DEFINE_PER_CPU_VAL(uint32_t, cpuid_extended_range_base, CPUID_EXT_RANGE_BASE_VAL);
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
    if (!cpuid_leaf_in_range(CPUID_BRAND_STRING_LEAF)) {
        pr_error("The brand string cpuid leaf is not supported");
        return false;
    }

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

    if (!cpuid_leaf_in_range(CPUID_CPU_FEATURES_LEAF)) {
        pr_error("Unable to query CPU feature information as the corresponding leaf is not supported");
        return false;
    }

    result = cpuid_raw(CPUID_CPU_FEATURES_LEAF, NO_SUBLEAF_INDEX);

    if (IS_CLEAR(result.ecx, CPUID_VMX)) {
        pr_error("The processor does not support the virtual machine extensions!");
        return false;
    }

    if (IS_CLEAR(result.edx, CPUID_MSR)) {
        pr_error("The processor does not support the 'rdmsr' and 'wrmsr' instructions");
        return false;
    }

    if (IS_CLEAR(result.edx, CPUID_LAPIC)) {
        pr_error("No local APIC present!");
        return false;
    }

    return true;
}

static inline bool
check_cpu(void)
{
    if (!cpuid_supported()) {
        pr_error("Procesor does not support CPUID");
        return false;
    }

    if (!is_genuine_intel()) {
        pr_error("Not a genuine Intel CPU");
        return false;
    }

    if (!all_cpu_features_supported()) {
        pr_error("Not all required CPU features are supported");
        return false;
    }

    return true;
}

static inline bool
enable_vmx_cr4(void)
{
    const uint64_t cr4 = read_cr4();
    write_cr4(SET_BIT(cr4, CR4_VMXE));
    return IS_SET(read_cr4(), CR4_VMXE);
}

static bool
enable_ia32_feature_ctrl_vmx_support(void)
{
    uint64_t ftr_ctrl = read_msr(MSRX64_IA32_FEATURE_CONTROL_MSR);

    if (IS_SET(ftr_ctrl, MSRX64_FTR_CTRL_LOCKED)) {

        if (IS_CLEAR(ftr_ctrl, MSRX64_FTR_CTRL_VMX_IN_SMX)) {
            /* Not relevant for us */
            pr_debug("VMX operation cannot be enabled in SMX operation");
        }

        if (IS_CLEAR(ftr_ctrl, MSRX64_FTR_CTRL_VMX_OUTSIDE_SMX)) {
            pr_error(
                    "Feature lock bit is already set and VMX operation is "
                    "configured to be prohibited outside of SMX operation"
            );
            return false;
        }

        return true;
    } else {
        /**
         * If the supported feature selection is not yet locked, make sure
         * VMX operation is supported in any case and then lock the selection
         */
        ftr_ctrl = SET_BIT(
                ftr_ctrl,
                MSRX64_FTR_CTRL_VMX_IN_SMX
                | MSRX64_FTR_CTRL_VMX_OUTSIDE_SMX
                | MSRX64_FTR_CTRL_LOCKED
        );

        write_msr(MSRX64_IA32_FEATURE_CONTROL_MSR, ftr_ctrl);
        return true;
    }

    return false;
}

static inline bool
enable_vmx_feature_support(void)
{
    if (!enable_vmx_cr4()) {
        pr_error("Unable to set VMX enable bit in CR4");
        return false;
    }

    if (!enable_ia32_feature_ctrl_vmx_support()) {
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

    logical_processor_t *processor = percpu_ptr(logical_processor);
    processor->raw_cr3 = *((const uint64_t *) &new_addr_space);

    write_cr3(*(const uint64_t *) &new_addr_space);

    return true;
}

void
__arch_setup_bsp(void)
{
    if (check_cpu() == false) {
        die_reason("Logical processor cannot support hyvemind operation");
    }

    /* We map this into the bootloader provided page tables for early bootstraping */
    const phys_addr_t lapic_base = get_lapic_base();
    if (identity_map_mmio_page(lapic_base) != 0) {
        die_reason("Unable to identity map the APIC page");
    }

    query_max_leaf_values();

    if (early_mm_init() != 0) {
        die_reason("Unable to identity basic cpu mm information");
    }

    if (create_addr_space() == false) {
        die_reason("Failed to the address space of the BSP");
    }

    if (init_new_gdt() != 0) {
        die_reason("Failed to create a GDT for the BSP");
    }

    if (install_new_tss() != 0) {
        die_reason("Failed to install a TSS into the GDT of the BSP");
    }

    if (load_gdt() != 0) {
        die_reason("Failed to load the GDT for the BSP");
    }

    init_shared_idt();
    load_shared_idt();

    if (!enable_vmx_feature_support()) {
        die_reason("Unable to enable VMX support for BSP");
    }
}

extern uint8_t boot_info_scratch[];

struct shared_boot_info {
    uint64_t raw_shared_cr3;
};

static void __no_return
arch_setup_ap(void)
{
    const struct shared_boot_info *info = (struct shared_boot_info *) &boot_info_scratch;
    write_cr3(info->raw_shared_cr3);

    query_max_leaf_values();

    logical_processor_t *current = get_current_logical_processor();
    current->raw_cr3 = info->raw_shared_cr3;
    current->lapic_id = read_lapic_id();
    current->processor_id = get_current_cpuid();

    init_new_gdt();
    install_new_tss();
    load_gdt();

    load_shared_idt();

    if (!enable_vmx_feature_support()) {
        current->state = PROCESSOR_UNAVAILABLE;
        die_reason("Cannot enable VMX support for processor");
    }

    current->state = PROCESSOR_INIT;
    /* rebase before we enter the idle loop and not here */
    arch_rebase_current_stack(DEFAULT_HYV_THREAD_STACK_SIZE, 1);
    die_reason("and dead.");
}

static void __no_return
arch_setup_ap_limine(const struct limine_mp_info *info)
{
    arch_setup_ap();
}

/**
 * Wrapper using limine provided AP information until we implement
 * AP initialization manually
 */
void
arch_bringup_aps_limine(const struct limine_mp_response *mp_info)
{
    const lapicid_t curr_lapic_id = read_lapic_id();
    const logical_processor_t *bsp = get_current_logical_processor();

    struct shared_boot_info *info = (struct shared_boot_info *) &boot_info_scratch;
    info->raw_shared_cr3 = bsp->raw_cr3;

    struct limine_mp_info  *entry;
    for (uint64_t i = 0; i < mp_info->cpu_count; i++) {
        entry = mp_info->cpus[i];

        if (curr_lapic_id == entry->lapic_id) {
            continue;
        }

        asm volatile (
                "movq %0, (%1)"
                :
                : "r"(&arch_setup_ap_limine), "r"(&entry->goto_address)
        );
    }
}

