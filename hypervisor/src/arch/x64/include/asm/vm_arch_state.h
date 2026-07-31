#ifndef _HYVEMIND_X64_ASM_VM_ARCH_STATE_H
#define _HYVEMIND_X64_ASM_VM_ARCH_STATE_H

#include "hyvstdlib.h"
#include "asm/segmentation.h"
#include "vmx/vmcs.h"

enum vcpu_cpu_mode {
    PROTECTED_MODE_32B,
    LONG_MODE_64B,
};

struct vcpu_segment {
    segment_selector_t selector;
    uint64_t base;
    uint32_t limit;
    guest_access_rights_t access_rights;
};

struct vcpu_sys_table {
    uint64_t base;
    uint32_t limit;
};

struct vcpu_user_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;

    union {
        uint64_t rbp;
        uint32_t ebp;
    };

    union {
        uint64_t rip;
        uint32_t eip;
    };

    union {
        uint64_t rsp;
        uint32_t esp;
    };

    union {
        uint64_t rflags;
        uint32_t eflags;
    };
};

struct vcpu_segments {
    struct vcpu_segment cs;
    struct vcpu_segment ss;
    struct vcpu_segment ds;
    struct vcpu_segment es;
    struct vcpu_segment fs;
    struct vcpu_segment gs;
    struct vcpu_segment ldtr;
    struct vcpu_segment tr;
};

#endif /* _HYVEMIND_X64_ASM_VM_ARCH_STATE_H */

