#ifndef _HYVEMIND_X64_ASM_PROCESSOR_H
#define _HYVEMIND_X64_ASM_PROCESSOR_H

#include "cpufeatures.h"
#include "types.h"
#include "per-cpu.h"

union cr0 {
    uint64_t raw;
    struct {
        uint64_t pe         : 1, /* Enables protected mode */
                 mp         : 1, /* Monitor Coprocessor */
                 em         : 1,
                 ts         : 1,
                 et         : 1,
                 ne         : 1,
                 reserved0  : 10,
                 wp         : 1, /* Write protection. Must be set before CR4.CET */
                 reserved1  : 1,
                 am         : 1, /* Enable automatic alignment checking */
                 reserved2  : 10,
                 nw         : 1,
                 cd         : 1,
                 pg         : 1, /* Enables paging */
                 reserved3  : 32;
    };
};

union cr4 {
    uint64_t raw;
    struct {
        uint64_t vme        : 1, /* Virtual-8086 Mode Extensions */
                 pvi        : 1, /* Protected Mode Virtual Interrupts */
                 tsd        : 1, /* Time Stamp Disable */
                 de         : 1, /* Debugging Extensions */
                 pse        : 1, /* Page Size Extensions */
                 pae        : 1, /* Physical Address Extension */
                 mce        : 1, /* Enables machine-check exception when set */
                 pge        : 1, /* Page Global Enable */
                 pce        : 1, /* Performance-Monitoring Counter Enable */
                 osfxsr     : 1,
                 osxmmexcpt : 1,
                 umip       : 1, /* User-Mode Instruction Prevention */
                 la57       : 1, /* When set use 5-lvl paging, otherwise 4-lvl paging */
                 vmxe       : 1, /* VMX-Enable Bit */
                 smxe       : 1, /* SMX-Enable Bit */
                 reserved0  : 1,
                 fsgsbase   : 1, /* FSGSBASE-Enable Bit */
                 pcide      : 1, /* Enable process-context identifiers */
                 osxsave    : 1,
                 kl         : 1,
                 smep       : 1, /* Enables supervisor-mode execution prevention */
                 smap       : 1, /* Enables supervisor-mode access prevention */
                 pke        : 1, /* Enable protection keys for user-mode pages */
                 cet        : 1, /* Control-flow Enforcement Technology */
                 pks        : 1, /* Enable protection keys for supervisor-mode pages */
                 uintr      : 1, /* User Interrupts Enable Bit */
                 reserved1  : 1,
                 lass       : 1,
                 lam_sup    : 1, /* Supervisor LAM enable */
                 reserved2  : 3,
                 fred       : 1,
                 reserved3  : 31;
    };
};

#define ACCESS_CONTROL_REGISTER(reg)                        \
    static inline uint64_t read_##reg(void) {               \
        uint64_t ret;                                       \
        asm volatile("mov %%" #reg ", %0" : "=r"(ret));     \
        return ret;                                         \
    }                                                       \
                                                            \
    static inline void write_##reg(const uint64_t reg) {    \
        asm volatile("mov %0, %%"#reg :: "r"(reg));         \
    }

ACCESS_CONTROL_REGISTER(cr0)
ACCESS_CONTROL_REGISTER(cr3)
ACCESS_CONTROL_REGISTER(cr4)
ACCESS_CONTROL_REGISTER(cr8)
#undef ACCESS_CONTROL_REGISTER

static inline uint64_t
read_msr(const uint64_t msr_nr)
{
    uint32_t edx;
    uint32_t eax;

    asm volatile(
        "rdmsr"
        : "=a"(eax), "=d"(edx)
        : "c"(msr_nr)
    );

    /* edx || eax */
    const uint64_t recombined = ((U64(0) | edx) << 32) | eax;
    return recombined;
}

static inline void
write_msr(const uint64_t msr_nr, const uint64_t val)
{
    const uint32_t edx = U64_UPPER32(val);
    const uint32_t eax = U64_LOWER32(val);
    asm volatile("wrmsr" :: "c"(msr_nr), "d"(edx), "a"(eax));
}

static inline uint64_t
read_rflags(void)
{
    uint64_t ret;

    asm volatile(
        "pushfq     \n\t"
        "pop %0     \n\t"
        : "=r"(ret)
    );

    return ret;
}

static inline void
write_rflags(uint64_t flags)
{
    asm volatile (
        "push %0    \n\t"
        "popfq      \n\t"
        :
        : "r"(flags)
    );
}

DECLARE_PER_CPU(uint32_t, cpuid_range_base);
DECLARE_PER_CPU(uint32_t, cpuid_max_leaf);
DECLARE_PER_CPU(uint32_t, cpuid_extended_range_base);
DECLARE_PER_CPU(uint32_t, cpuid_max_extended_leaf);

/* Marker to indicate when the ecx value is irrelevant */
#define NO_SUBLEAF_INDEX 0

struct cpuid_result {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};
typedef struct cpuid_result cpuid_result_t;

static inline void
__cpuid(const uint32_t ida, const uint32_t idc, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    asm volatile (
            "cpuid"
            : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
            : "a"(ida), "c"(idc)
    );
}

/**
 * Does not check whether the queried leaf is valid
 */
static inline cpuid_result_t
cpuid_raw(const uint32_t eax, const uint32_t ecx)
{
    cpuid_result_t result;
    __cpuid(eax, ecx, &result.eax, &result.ebx, &result.ecx, &result.edx);
    return result;
}

static inline int
cpuid(const uint32_t eax, const uint32_t ecx, cpuid_result_t *result)
{
    const bool in_base_range = (percpu_val(cpuid_range_base) <= eax &&
                                percpu_val(cpuid_max_leaf) >= eax);

    const bool in_ext_range = (percpu_val(cpuid_extended_range_base) <= eax &&
                               percpu_val(cpuid_max_extended_leaf) >= eax);

    if (in_base_range || in_ext_range) {
        __cpuid(eax, ecx, &result->eax, &result->ebx, &result->ecx, &result->edx);
        return 0;
    }

    return -1;
}

static inline bool
cpuid_leaf_in_range(const uint32_t leaf)
{
    cpuid_result_t res;

    res = cpuid_raw(CPUID_BASE_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    if (CPUID_RANGE_BASE_VAL <= leaf && leaf <= res.eax) {
        return true;
    }

    res = cpuid_raw(CPUID_EXTENDED_RANGE_LIMITS_LEAF, NO_SUBLEAF_INDEX);
    if (CPUID_EXT_RANGE_BASE_VAL <= leaf && leaf <= res.eax) {
        return true;
    }

    return false;
}

static inline void
enable_local_interrupts(void)
{
    asm volatile ("sti");
}

static inline void
disable_local_interrupts(void)
{
    asm volatile ("cli");
}

#endif /* _HYVEMIND_X64_ASM_PROCESSOR_H */

