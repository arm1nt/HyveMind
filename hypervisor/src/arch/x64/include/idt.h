#ifndef _HYVEMIND_X64_IDT_H
#define _HYVEMIND_X64_IDT_H

#include "mm.h"
#include <stdint.h>

enum {
    TI_GDT = 0,
    TI_LDT = 1
};

enum {
    SYSTEM_SEG_DESC = 0,
    CODE_DATA_SEG_DESC = 1
};

enum {
    SEGMENT_NOT_PRESENT = 0,
    SEGMENT_PRESENT = 1
};

#define SEG_DATA_TYPE_RO        0
#define SEG_DATA_TYPE_ROA       1
#define SEG_DATA_TYPE_RW        2
#define SEG_DATA_TYPE_RWA       3
#define SEG_DATA_TYPE_RO_DOWN   4
#define SEG_DATA_TYPE_ROA_DOWN  5
#define SEG_DATA_TYPE_RW_DOWN   6
#define SEG_DATA_TYPE_RWA_DOWN  7

/**
 * X .. Execute only
 * XR .. Execute/Read
 * A ... accessed
 * C ... conforming
 *
 * TODO: properly name the macros
 */
#define SEG_CODE_TYPE_X         8
#define SEG_CODE_TYPE_XA        9
#define SEG_CODE_TYPE_XR        10
#define SEG_CODE_TYPE_XRA       11
#define SEG_CODE_TYPE_XC        12
#define SEG_CODE_TYPE_XCA       13
#define SEG_CODE_TYPE_XRC       14
#define SEG_CODE_TYPE_XRCA      15

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

#define GDT_ENTRIES 2

struct gdt_struct {
    segment_descriptor_t gdt[GDT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE)));

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
    segment_selector_t segment_selector;
    struct idt_bits idt_bits;
    uint16_t mid_offset;
    uint32_t high_offset;
    uint32_t reserved;
} __attribute__((packed));
typedef struct idt_gate idt_gate_t;

static inline uint64_t
get_idt_gate_offset(const struct idt_gate *gate)
{
    // TOOD:
    return -1;
}

/**
 * Use to store current segment registers, and/or swap out the current segment
 * register state
 */
struct segment_regs {
    segment_selector_t cs;
    segment_selector_t ds;
    segment_selector_t ss;
    segment_selector_t gs;
    segment_selector_t fs;
    segment_selector_t es;
};

/* TODO: refactor properly */

static segment_descriptor_t hyvemind_hypervisor_cs_segment_desc = {
    .limit0 = 0,
    .limit1 = 0,
    .low_base = 0,
    .mid_base = 0,
    .high_base = 0,
    .s = CODE_DATA_SEG_DESC,
    .type = SEG_CODE_TYPE_X,
    .dpl = 0,
    .p = SEGMENT_PRESENT,
    .avl = 0,
    .l = 1, /* 64 bit code segment */
    .db = 0,
    .g = 0
};

#define HYVEMIND_HYPERVISOR_CS        1
#define HYVEMIND_HYPERVISOR_DS        2

static segment_selector_t hyvemind_hypervisor_cs_segment_selector = {
    .index = HYVEMIND_HYPERVISOR_CS,
    .ti = TI_GDT,
    .rpl = 0
};

static segment_descriptor_t hyvemind_hypervisor_ds_segment_desc;

int install_idt(void);

int install_gdt(void);

#endif /* _HYVEMIND_X64_IDT_H */

