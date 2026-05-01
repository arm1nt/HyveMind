#ifndef _HYVEMIND_X64_PROCESSOR_H
#define _HYVEMIND_X64_PROCESSOR_H

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

#endif /* _HYVEMIND_X64_PROCESSOR_H */

