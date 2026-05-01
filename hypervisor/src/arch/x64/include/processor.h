#ifndef _HYVEMIND_X64_PROCESSOR_H
#define _HYVEMIND_X64_PROCESSOR_H

#include "types.h"
#include <stdint.h>

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

ACCESS_CONTROL_REGISTER(cr4)
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
    const uint32_t edx = val >> 32;
    const uint32_t eax = (uint32_t) ((val << 32) >> 32);
    asm volatile("wrmsr" :: "c"(msr_nr), "d"(edx), "a"(eax));
}

#endif /* _HYVEMIND_X64_PROCESSOR_H */

