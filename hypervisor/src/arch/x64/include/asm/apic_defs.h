#ifndef _HYVEMIND_X64_ASM_APIC_DEFS_H
#define _HYVEMIND_X64_ASM_APIC_DEFS_H

#include "types.h"

#define APIC_MEM_BASE   U64_LSHIFT(0xFEE00000, 12)
#define APIC_MSR_BASE   0x800

#define APIC_ID_REG             0x020
#define APIC_VERSION_REG        0x030
#define APIC_TPR_REG            0x080
#define APIC_PPR_REG            0x0A0
#define APIC_EOI_REG            0x0B0
#define APIC_LDR_REG            0x0D0
#define APIC_SVR_REG            0x0F0
#define APIC_ISR_REG_W1         0x100
#define APIC_ISR_REG_W2         0x110
#define APIC_ISR_REG_W3         0x120
#define APIC_ISR_REG_W4         0x130
#define APIC_ISR_REG_W5         0x140
#define APIC_ISR_REG_W6         0x150
#define APIC_ISR_REG_W7         0x160
#define APIC_ISR_REG_W8         0x170
#define APIC_TMR_REG_W1         0x180
#define APIC_TMR_REG_W2         0x190
#define APIC_TMR_REG_W3         0x1A0
#define APIC_TMR_REG_W4         0x1B0
#define APIC_TMR_REG_W5         0x1C0
#define APIC_TMR_REG_W6         0x1D0
#define APIC_TMR_REG_W7         0x1E0
#define APIC_TMR_REG_W8         0x1F0
#define APIC_IRR_REG_W1         0x200
#define APIC_IRR_REG_W2         0x210
#define APIC_IRR_REG_W3         0x220
#define APIC_IRR_REG_W4         0x230
#define APIC_IRR_REG_W5         0x240
#define APIC_IRR_REG_W6         0x250
#define APIC_IRR_REG_W7         0x260
#define APIC_IRR_REG_W8         0x270
#define APIC_ERROR_STATUS_REG   0x280
#define APIC_LVT_CMCI_REG       0x2F0
#define APIC_LVT_TIMER_REG      0x320
#define APIC_LVT_THERMAL_REG    0x330
#define APIC_LVT_PERFMON_REG    0x340
#define APIC_LVT_LINT0_REG      0x350
#define APIC_LVT_LINT1_REG      0x360
#define APIC_LVT_ERROR_REG      0x370
#define APIC_INITIAL_COUNT_REG  0x380
#define APIC_CURR_COUNT_REG     0x390
#define APIC_DIV_CONFIG_REG     0x3E0

#define XAPIC_APR_REG           0x0A0
#define XAPIC_RRD_REG           0x0C0
#define XAPIC_DEST_FORMAT_REG   0x0E0
#define XAPIC_ICR_REG_W1        0x300
#define XAPIC_ICR_REG_W2        0x310

#define X2APIC_ICR_REG          0x300
#define X2APIC_SELF_IPI_REG     0x3F0

/* Must be 0, any other value causes a GP */
#define APIC_EOI_ACK    0x00

#define APIC_LVT_RESET_VAL      0x00010000
#define APIC_LVT_MASK_BIT       16
#define APIC_LVT_TIMER_MODE_POS 17

#define APIC_TSC_DEADLINE_MSR   0x6E0

enum apic_timer_type {
    APIC_ONESHOT_TIMER,
    APIC_PERIODIC_TIMER,
    APIC_TSC_DEADLINE_TIMER,
};

enum apic_frequency_divisor {
    APIC_FREQ_DIV_2,
    APIC_FREQ_DIV_4,
    APIC_FREQ_DIV_8,
    APIC_FREQ_DIV_16,
    APIC_FREQ_DIV_32,
    APIC_FREQ_DIV_64,
    APIC_FREQ_DIV_128,
    APIC_FREQ_DIV_1,
};

#define APIC_SVR_RESET_VAL  0xFF

union apic_svr {
    uint32_t raw;
    struct {
        uint32_t vector                     : 8,
                 sw_enabled                 : 1,
                 focus_checking_disabled    : 1,
                 reserved0                  : 2,
                 eoi_broadcast_suppression  : 1,
                 reserved1                  : 19;
    };
};
typedef union apic_svr apic_svr_t;

union apic_icr {
    uint64_t raw;
    struct {
        uint64_t vector             : 8,
                 delivery_mode      : 3,
                 destination_mode   : 1,
                 delivery_status    : 1,
                 reserved0          : 1,
                 level              : 1,
                 trigger_mode       : 1,
                 reserved1          : 2,
                 destination_short  : 2,
                 reserved2          : 12,
                 destination        : 32;
    };
};
typedef union apic_icr apic_icr_t;

enum apic_task_priority {
    TP_CLASS_0,
    TP_CLASS_1,
    TP_CLASS_2,
    TP_CLASS_3,
    TP_CLASS_4,
    TP_CLASS_5,
    TP_CLASS_6,
    TP_CLASS_7,
    TP_CLASS_8,
    TP_CLASS_9,
    TP_CLASS_10,
    TP_CLASS_11,
    TP_CLASS_12,
    TP_CLASS_13,
    TP_CLASS_14,
    TP_CLASS_15,
};

union ia32_apic_base_msr {
    uint64_t raw;
    struct {
        uint64_t reserved0      : 8,
                 bsp            : 1,
                 reserved1      : 1,
                 extd           : 1,
                 global_enable  : 1,
                 base           : 24,
                 reserved2      : 28;
    };
};
typedef union ia32_apic_base_msr apic_base_msr_t;

#endif /* _HYVEMIND_X64_ASM_APIC_DEFS_H */

