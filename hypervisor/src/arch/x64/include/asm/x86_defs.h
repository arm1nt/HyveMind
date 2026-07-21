#ifndef _HYVEMIND_X64_ASM_X86_DEFS_H
#define _HYVEMIND_X64_ASM_X86_DEFS_H

#include "stdint.h"

union cr0 {
    uint64_t raw;
    struct {
        uint64_t pe         : 1,  /* Enables protected mode */
                 mp         : 1,  /* Monitor Coprocessor */
                 em         : 1,
                 ts         : 1,
                 et         : 1,
                 ne         : 1,
                 reserved0  : 10,
                 wp         : 1,  /* Write protection. Must be set before CR4.CET */
                 reserved1  : 1,
                 am         : 1,  /* Enable automatic alignment checking */
                 reserved2  : 10,
                 nw         : 1,
                 cd         : 1,
                 pg         : 1,  /* Enables paging */
                 reserved3  : 32;
    };
};
typedef union cr0 cr0_t;

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
typedef union cr4 cr4_t;

union rflags {
    uint64_t raw;
    struct {
        uint64_t cf         : 1,
                 reserved0  : 1, /* must be 1 */
                 pf         : 1,
                 reserved1  : 1,
                 af         : 1,
                 reserved2  : 1,
                 zf         : 1,
                 sf         : 1,
                 tf         : 1,
                 intrf      : 1,
                 df         : 1,
                 of         : 1,
                 iopl       : 1,
                 nt         : 1,
                 reserved3  : 1,
                 rf         : 1,
                 vm         : 1,
                 ac         : 1,
                 vif        : 1,
                 vip        : 1,
                 id         : 1,
                 reserved4  : 42;
    };
};
typedef union rflags rflags_t;

union ia32_efer {
    uint64_t raw;
    struct {
        uint64_t syscall_enable : 1,
                 reserved0      : 7,
                 lme            : 1,
                 reserved1      : 1,
                 lma            : 1,
                 nxe            : 1,
                 reserved2      : 52;
    };
};
typedef union ia32_efer ia32_efer_t;

#endif /* _HYVEMIND_X64_ASM_X86_DEFS_H */

