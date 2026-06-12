#ifndef _HYVEMIND_X64_ASM_TSS_H
#define _HYVEMIND_X64_ASM_TSS_H

#include "asm/paging.h"

struct tss_descriptor {
    uint16_t limit0;
    uint16_t base0;
    uint16_t base1      : 8,
             type       : 4,
             reserved0  : 1,
             dpl        : 2,
             p          : 1;
    uint16_t limit1     : 4,
             avl        : 1,
             reserved1  : 2,
             g          : 1,
             base2      : 8;
    uint32_t base3;
    uint32_t reserved2;
} __attribute__((packed));
typedef struct tss_descriptor tss_descriptor_t;

#define TSS_RSP_ENTRY(name) \
    uint32_t name##_low;   \
    uint32_t name##_high

#define TSS_IST_ENTRY(name) \
    uint32_t name##_low; \
    uint32_t name##_high

#define TSS_IST_DEFAULT_SIZE_PAGES  4
enum {
    TSS_IST_INDEX0,
    TSS_IST_INDEX1,
    TSS_IST_INDEX2,
    TSS_IST_INDEX3,
    TSS_IST_INDEX4,
    TSS_IST_INDEX5,
    TSS_IST_INDEX6,
};

struct tss {
    uint32_t reserved0;
    TSS_RSP_ENTRY(rsp0);
    TSS_RSP_ENTRY(rsp1);
    TSS_RSP_ENTRY(rsp2);
    uint32_t reserved1;
    uint32_t reserved2;
    TSS_IST_ENTRY(ist1);
    TSS_IST_ENTRY(ist2);
    TSS_IST_ENTRY(ist3);
    TSS_IST_ENTRY(ist4);
    TSS_IST_ENTRY(ist5);
    TSS_IST_ENTRY(ist6);
    TSS_IST_ENTRY(ist7);
    uint32_t reserved3;
    uint32_t reserved4;
    uint16_t reserved5;
    uint16_t io_map_base_addr;
} __attribute__((aligned(PAGE_SIZE)));
typedef struct tss tss_t;

#undef TSS_RSP_ENTRY
#undef TSS_IST_ENTRY

int init_default_tss(tss_t *tss, const unsigned int stack_size_pages);

tss_descriptor_t create_tss_desc(const tss_t *tss_segment, const unsigned int dpl);

#endif /* _HYVEMIND_X64_ASM_TSS_H */

