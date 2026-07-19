#ifndef _HYVEMIND_X64_ASM_SEGMENTATION_H
#define _HYVEMIND_X64_ASM_SEGMENTATION_H

#include <stdint.h>
#include <asm/tss.h>

enum {
    TI_GDT = 0,
    TI_LDT = 1,
};

enum descriptor_type {
    SYSTEM_SEGMENT_DESC     = 0,
    CODE_DATA_SEGMENT_DESC  = 1,
};
typedef  enum descriptor_type desc_type_t;

enum {
    SEGMENT_NOT_PRESENT = 0,
    SEGMENT_PRESENT     = 1,
};

enum code_data_segment_type {
    DATA_RO,
    DATA_RO_ACCESSED,
    DATA_RW,
    DATA_RW_ACCESSED,
    DATA_RO_EXPAND_DOWN,
    DATA_RO_EXPAND_DOWN_ACCESSED,
    DATA_RW_EXPAND_DOWN,
    DATA_RW_EXPAND_DOWN_ACCESSED,

    CODE_EXECUTE_ONLY,
    CODE_EXECUTE_ONLY_ACCESSED,
    CODE_EXECUTE_READ,
    CODE_EXECUTE_READ_ACCESSED,
    CODE_EXECUTE_ONLY_CONFORMING,
    CODE_EXECUTE_ONLY_CONFORMING_ACCESSED,
    CODE_EXECUTE_READ_CONFORMING,
    CODE_EXECUTE_READ_CONFORMING_ACCESSED,
};
typedef enum code_data_segment_type code_data_seg_type_t;

enum system_segment_type {
    IA32E_LDT               = 2,
    IA32E_TSS               = 9,
    IA32E_INTERRUPT_GATE    = 14,
    IA32E_TRAP_GATE         = 15,
};
typedef enum system_segment_type sys_seg_type_t;

struct segment_selector {
    uint16_t rpl    : 2,
             ti     : 1,
             index  : 13;
} __attribute__((packed));
typedef struct segment_selector segment_selector_t;

struct segment_descriptor {
    uint16_t limit0;
    uint16_t low_base;
    uint16_t mid_base   : 8,
             type       : 4,
             s          : 1,
             dpl        : 2,
             p          : 1;
    uint16_t limit1     : 4,
             avl        : 1,
             l          : 1,
             db         : 1,
             g          : 1,
             high_base  : 8;
} __attribute__((packed));
typedef struct segment_descriptor segment_descriptor_t;

#define DEFINE_SEGMENT_DESCRIPTOR(_desc_type, _seg_type, _dpl, _l)  \
    {                                                               \
        .limit0 = 0,                                                \
        .limit1 = 0,                                                \
        .low_base = 0,                                              \
        .mid_base = 0,                                              \
        .high_base = 0,                                             \
        .s = (_desc_type),                                          \
        .type = (_seg_type),                                        \
        .dpl = (_dpl),                                              \
        .p = SEGMENT_PRESENT,                                       \
        .avl = 0,                                                   \
        .l = (_l),                                                  \
        .db = 0,                                                    \
        .g = 0                                                      \
    }

#define DEFINE_SEGMENT_SELECTOR(_index, _ti, _rpl)  \
    {                                               \
        .index = (_index),                          \
        .ti = (_ti),                                \
        .rpl = (_rpl)                               \
    }

#define NULL_SEG_SELECTOR DEFINE_SEGMENT_SELECTOR(0,0,0)

struct segment_regs {
    segment_selector_t cs;
    segment_selector_t ds;
    segment_selector_t es;
    segment_selector_t fs;
    segment_selector_t gs;
    segment_selector_t ss;
};

void reload_segment_registers(const struct segment_regs *regs);

void reload_tr_register(const segment_selector_t *selector);
segment_selector_t read_task_register(void);

segment_selector_t get_cs(void);

int get_cpl(void);

#endif /* _HYVEMIND_X64_ASM_SEGMENTATION_H */

