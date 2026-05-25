#include "arch_init.h"
#include "cpufeatures.h"
#include "fatal.h"
#include "gdt_idt.h"
#include "printf.h"
#include "processor.h"
#include "string.h"
#include "asm/mm.h"

#include <stdbool.h>

const uint32_t cpuid_range_base = 0x00;
uint32_t cpuid_max_leaf;
const uint32_t cpuid_extended_range_base = 0x80000000;
uint32_t cpuid_max_extended_leaf;

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

    result = cpuid_raw(CPUID_BASE_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    cpuid_max_leaf = result.eax;

    result = cpuid_raw(CPUID_EXTENDED_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    cpuid_max_extended_leaf = result.eax;
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
        printf("The CPU does not support the virtual machine extensions!");
        return false;
    }

    if (IS_CLEAR(result.edx, CPUID_MSR)) {
        printf("The CPU does not support the 'rdmsr' and 'wrmsr' instructions");
        return false;
    }

    return true;
}

static inline int
check_cpu(void)
{
    if (!cpuid_supported()) {
        printf("CPU does not support CPUID");
        return -1;
    }

    query_max_leaf_values();

    if (!is_genuine_intel()) {
        printf("Not a genuine Intel CPU");
        return -1;
    }

    if (!all_cpu_features_supported()) {
        printf("Not all required CPU features are supported");
        return -1;
    }

    return 0;
}

void
arch_init(
        const struct limine_memmap_response *mem_map,
        const struct limine_executable_address_response *exec_addr_info
)
{
    printf("Starting X64 specific initialization...");

    if (check_cpu() != 0) {
        printf("CPU not suited for running hyvemind");
        die();
    }

    if (setup_gdt_idt() != 0) {
        printf("Failed to install IDT and GDT");
        die();
    }

    if (init_mm(mem_map, exec_addr_info) != 0) {
        printf("Memory management initialization failed");
        die();
    }

}

