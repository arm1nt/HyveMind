#include "arch_init.h"
#include "cpufeatures.h"
#include "fatal.h"
#include "gdt_idt.h"
#include "printf.h"
#include "processor.h"

#include <stdbool.h>

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

static inline int
check_cpu(void)
{
    if (!cpuid_supported()) {
        printf("CPU does not support CPUID");
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

