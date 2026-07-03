#ifndef _HYVEMIND_ASM_X64_GDT_IDT_H
#define _HYVEMIND_ASM_X64_GDT_IDT_H

#include "asm/paging.h"
#include "asm/segmentation.h"

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

#define HYVEMIND_CS_SEGMENT_INDEX       1
#define HYVEMIND_DATA_SEGMENT_INDEX     2
#define HYVEMIND_TSS_INDEX              3

#define GDT_ENTRIES 5
#define IDT_ENTRIES 256

struct idt_struct {
    uint64_t gates[IDT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE)));

struct gdt_struct {
    uint64_t gdt[GDT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE)));

#define GDT_LIMIT(x) (((x)*8)-1)
#define IDT_LIMIT(x) (((x)*16)-1)

#define GDT_SELECTOR(_index, _ti, _rpl) \
    DEFINE_SEGMENT_SELECTOR(_index, _ti, _rpl)

#define GDT_NULL_SELECTOR GDT_SELECTOR(0, 0, 0)

static segment_descriptor_t hyvemind_cs_segment_desc =
    DEFINE_SEGMENT_DESCRIPTOR(CODE_DATA_SEGMENT_DESC, CODE_EXECUTE_ONLY, 0, 1);

static segment_descriptor_t hyvemind_data_segment_desc =
    DEFINE_SEGMENT_DESCRIPTOR(CODE_DATA_SEGMENT_DESC, DATA_RW_EXPAND_DOWN, 0, 0);

void init_new_gdt(void);
int install_new_tss(void);
void load_gdt(void);

void init_shared_idt(void);
void load_shared_idt(void);

#endif /* _HYVEMIND_ASM_X64_GDT_IDT_H */

