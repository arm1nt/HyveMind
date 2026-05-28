#ifndef _HYVEMIND_X64_GDT_IDT_H
#define _HYVEMIND_X64_GDT_IDT_H

#include "types.h"
#include "asm/paging.h"
#include <stdint.h>

enum {
    TABLE_INDICATOR_GDT = 0,
    TABLE_INDICATOR_IDT = 1,
};

enum {
    SYSTEM_SEGMENT_DESC     = 0,
    CODE_DATA_SEGMENT_DESC  = 1,
};

enum {
    SEGMENT_NOT_PRESENT = 0,
    SEGMENT_PRESENT     = 1,
};

struct segment_selector {
    uint16_t rpl    : 2,
             ti     : 1,
             index  : 13;
} __attribute__((packed));
typedef struct segment_selector segment_selector_t;

struct segment_regs {
    segment_selector_t cs;
    segment_selector_t ds;
    segment_selector_t es;
    segment_selector_t fs;
    segment_selector_t gs;
    segment_selector_t ss;
};

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

struct idt_bits {
    uint16_t ist    : 3,
             zero1  : 5,
             type   : 4,
             zero2  : 1,
             dpl    : 2,
             p      : 1;
} __attribute__((packed));

struct idt_gate {
    uint16_t low_offset;
    segment_selector_t selector;
    struct idt_bits idt_bits;
    uint16_t mid_offset;
    uint32_t high_offset;
    uint32_t reserved;
} __attribute__((packed));
typedef struct idt_gate idt_gate_t;

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));
typedef struct idt_ptr idt_ptr_t;

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));
typedef struct gdt_ptr gdt_ptr_t;

#define IDT_ENTRIES 256
#define GDT_ENTRIES 3

struct idt_struct {
    idt_gate_t gates[IDT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE)));

struct gdt_struct {
    segment_descriptor_t gdt[GDT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE)));

#define GDT_LIMIT(x) ((x*8)-1)
#define IDT_LIMIT(x) ((x*16)-1)

#define SEGMENT_DATA_TYPE_RO                0
#define SEGMENT_DATA_TYPE_RO_ACCESSED       1
#define SEGMENT_DATA_TYPE_RW                2
#define SEGMENT_DATA_TYPE_RW_ACCESSED       3
#define SEGMENT_DATA_TYPE_RO_DOWN           4
#define SEGMENT_DATA_TYPE_RO_ACCESSED_DOWN  5
#define SEGMENT_DATA_TYPE_RW_DOWN           6
#define SEGMENT_DATA_TYPE_RW_ACCESSED_DOWN  7

#define SEGMENT_CODE_TYPE_X                         8
#define SEGMENT_CODE_TYPE_X_ACCESSED                9
#define SEGMENT_CODE_TYPE_RX                        10
#define SEGMENT_CODE_TYPE_RX_ACCESSED               11
#define SEGMENT_CODE_TYPE_X_CONFORMING              12
#define SEGMENT_CODE_TYPE_X_CONFORMING_ACCESSED     13
#define SEGMENT_CODE_TYPE_RX_CONFORMING             14
#define SEGMENT_CODE_TYPE_RX_CONFORMING_ACCESSED    15

#define IA32E_LDT_TYPE              2
#define IA32E_INTERRUPT_GATE_TYPE   14
#define IA32E_TRAP_GATE_TYPE        15

#define DEFINE_SEGMENT_DESCRIPTOR(_s, _type, _dpl, _l)  \
    {                                                   \
        .limit0 = 0,                                    \
        .limit1 = 0,                                    \
        .low_base = 0,                                  \
        .mid_base = 0,                                  \
        .high_base = 0,                                 \
        .s = _s,                                        \
        .type = _type,                                  \
        .dpl = _dpl,                                    \
        .p = SEGMENT_PRESENT,                           \
        .avl = 0,                                       \
        .l = _l,                                        \
        .db = 0,                                        \
        .g = 0                                          \
    }

#define DEFINE_SEGMENT_SELECTOR(_index, _ti, _rpl)  \
    {                                               \
        .index = _index,                            \
        .ti = _ti,                                  \
        .rpl = _rpl                                 \
    }

#define INTERRUPT_IDT_BITS                  \
    (struct idt_bits)                       \
    {                                       \
        .zero1 = 0,                         \
        .zero2 = 0,                         \
        .dpl = 0,                           \
        .p = SEGMENT_PRESENT,               \
        .type = IA32E_INTERRUPT_GATE_TYPE,  \
        .ist = 0                            \
    }

static segment_descriptor_t hyvemind_cs_segment_desc =
    DEFINE_SEGMENT_DESCRIPTOR(CODE_DATA_SEGMENT_DESC, SEGMENT_CODE_TYPE_X, 0, 1);

static segment_descriptor_t hyvemind_data_segment_desc =
    DEFINE_SEGMENT_DESCRIPTOR(CODE_DATA_SEGMENT_DESC, SEGMENT_DATA_TYPE_RW_DOWN, 0, 0);

#define HYVEMIND_CS_SEGMENT_INDEX   1
#define HYVEMIND_DATA_SEGMENT_INDEX 2

static segment_selector_t hyvemind_cs_selector =
    DEFINE_SEGMENT_SELECTOR(HYVEMIND_CS_SEGMENT_INDEX, TABLE_INDICATOR_GDT, 0);

static segment_selector_t hyvemind_data_selector =
    DEFINE_SEGMENT_SELECTOR(HYVEMIND_DATA_SEGMENT_INDEX, TABLE_INDICATOR_GDT, 0);

static segment_selector_t gdt_null_selector = DEFINE_SEGMENT_SELECTOR(0, 0, 0);

static inline void
set_idt_gate_offset(idt_gate_t *gate, const uint64_t offset)
{
    const uint32_t lower32 = U64_LOWER32(offset);
    gate->low_offset = U32_LOWER16(lower32);
    gate->mid_offset = U32_UPPER16(lower32);
    gate->high_offset = U64_UPPER32(offset);
}


int setup_gdt_idt(void);

#endif /* _HYVEMIND_X64_GDT_IDT_H */

