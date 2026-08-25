#ifndef _HYVEMIND_X64_ASM_APIC_DEFS_H
#define _HYVEMIND_X64_ASM_APIC_DEFS_H

#include "types.h"

#define APIC_MEM_BASE   U64_LSHIFT(0xFEE00000, 12)
#define APIC_MSR_BASE   0x800

#define APIC_ID_REG             0x020
#define APIC_EOI_REG            0x0B0
#define APIC_SVR_REG            0x0F0
#define APIC_LVT_CMCI_REG       0x2F0
#define APIC_LVT_TIMER_REG      0x320
#define APIC_LVT_THERMAL_REG    0x330
#define APIC_LVT_PERFMON_REG    0x340
#define APIC_LVT_LINT0_REG      0x350
#define APIC_LVT_LINT1_REG      0x360
#define APIC_LVT_ERROR_REG      0x370

/* Must be 0, any other value causes a GP */
#define APIC_EOI_ACK    0x00

#define APIC_LVT_RESET_VAL      0x00010000
#define APIC_LVT_MASK_BIT       16
#define APIC_LVT_TIMER_MODE_POS 17

enum apic_timer_type {
    APIC_ONESHOT_TIMER,
    APIC_PERIODIC_TIMER,
    APIC_TSC_DEADLINE_TIMER,
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

#endif /* _HYVEMIND_X64_ASM_APIC_DEFS_H */

