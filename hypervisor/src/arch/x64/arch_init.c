#include "arch_init.h"
#include "cpufeatures.h"
#include "fatal.h"
#include "gdt_idt.h"
#include "printf.h"
#include "processor.h"
#include "string.h"

#include <stdbool.h>

uint32_t max_leaf;
uint32_t max_extended_leaf;

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
    const cpuid_result_t result = cpuid(0, 0);
    max_leaf = result.eax;
}

static inline bool
is_genuine_intel(void)
{
    const cpuid_result_t result = cpuid(0, 0);

    if (memcmp("Genu", (char *) &result.ebx, 4) != 0
        || memcmp("ineI", (char *) &result.edx, 4) != 0
        || memcmp("ntel", (char *) &result.ecx, 4) != 0) {
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

    return 0;
}

void
arch_init(void)
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
}

