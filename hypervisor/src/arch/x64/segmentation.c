#include "fatal.h"
#include "types.h"
#include "string.h"
#include "pf_alloc.h"
#include "printf.h"
#include "asm/segmentation.h"
#include "asm/paging.h"

extern void reload_cs_register(const segment_selector_t cs_selector);

segment_selector_t
read_ldtr(void)
{
    segment_selector_t selector;
    asm volatile ("SLDT %0" : "=m"(selector));
    return selector;
}

segment_selector_t
read_task_register(void)
{
    segment_selector_t tss_selector;
    asm volatile("STR %0" : "=m"(tss_selector));
    return tss_selector;
}

#define READ_SEGMENT_REG(_reg, _selector) \
    asm volatile ("mov %%" #_reg ", %0" : "=m"(_selector))

segment_selector_t
read_segment_register(const enum x86_segment_reg reg)
{
    segment_selector_t selector;

    switch (reg) {
        case X86_CS_REG:
            READ_SEGMENT_REG(cs, selector);
            break;
        case X86_SS_REG:
            READ_SEGMENT_REG(ss, selector);
            break;
        case X86_DS_REG:
            READ_SEGMENT_REG(ds, selector);
            break;
        case X86_ES_REG:
            READ_SEGMENT_REG(es, selector);
            break;
        case X86_FS_REG:
            READ_SEGMENT_REG(fs, selector);
            break;
        case X86_GS_REG:
            READ_SEGMENT_REG(gs, selector);
            break;
        case X86_LDTR_REG:
            selector = read_ldtr();
            break;
        case X86_TR_REG:
            selector = read_task_register();
            break;
        default:
            pr_error("Cannot read unknown segment register '%lu'", U64(reg));
            die();
    }

    return selector;
}

#undef READ_SEGMENT_REG

inline void
load_tr_register(const segment_selector_t *selector)
{
    asm volatile ("LTR %0" :: "m"(*selector));
}

void
load_segment_registers(const struct segment_regs *regs)
{
    reload_cs_register(regs->cs);

    asm volatile (
            "mov %0, %%ss\n\t"
            "mov %1, %%ds\n\t"
            "mov %2, %%es\n\t"
            "mov %3, %%fs\n\t"
            "mov %4, %%gs\n\t"
            :: "rm"(regs->ss), "rm"(regs->ds), "rm"(regs->es), "rm"(regs->fs), "rm"(regs->gs)
    );
}

static inline int
allocate_tss_ist(tss_t *tss, const enum tss_ist_index index, const uint64_t nr_stack_pages)
{
    virt_addr_t stack_bot, stack_start;

    if (index == TSS_IST_NO_IST_STACK) {
        pr_error("Index 0 does not refer to a stack");
        return -1;
    }

    if (get_pages_zeroed(nr_stack_pages, &stack_bot) != 0) {
        pr_error("Failed to allocate '%lu' pages for IST_%lu stack",
                U64(nr_stack_pages),
                U64(index)
        );
        return -1;
    }

    stack_start = stack_bot + (nr_stack_pages * PAGE_SIZE) - 8;

    tss->ist[index - 1].low = U64_LOWER32(stack_start);
    tss->ist[index - 1].high = U64_UPPER32(stack_start);

    return 0;
}

int
init_default_tss(tss_t *tss, const unsigned int nr_stack_pages)
{
    memset(tss, 0, sizeof(tss_t));

    if (allocate_tss_ist(tss, TSS_IST_INDEX1, nr_stack_pages) != 0) {
        pr_error("TSS initialization failed cause of an error allocating a IST1 stack");
        return -1;
    }

    if (allocate_tss_ist(tss, TSS_IST_INDEX2, nr_stack_pages) != 0) {
        pr_error("TSS initialization failed cause of an error allocating a IST2 stack");
        return -1;
    }

    return 0;
}

virt_addr_t
get_base_from_tss_descriptor(const tss_descriptor_t *desc)
{
    virt_addr_t base = desc->base0;
    base |= (U64_LSHIFT(desc->base1, 16));
    base |= (U64_LSHIFT(desc->base2, 24));
    base |= (U64_LSHIFT(desc->base3, 32));
    return base;
}

static inline void
set_tss_desc_limit(tss_descriptor_t *desc, const uint32_t limit)
{
    desc->limit0 = limit & 0xFFFF;
    desc->limit1 = (limit >> 16) & 0xF;
}

static void
set_tss_desc_base(tss_descriptor_t *desc, const virt_addr_t base)
{
    const uint32_t lower32 = U64_LOWER32(base);
    const uint16_t first16 = U32_LOWER16(lower32);
    const uint16_t second16 = U32_UPPER16(lower32);

    desc->base0 = first16;
    desc->base1 = U16_LOWER8(second16);
    desc->base2 = U16_UPPER8(second16);
    desc->base3 = U64_UPPER32(base);
}

tss_descriptor_t
create_tss_desc(const tss_t *tss_segment, const uint8_t dpl)
{
    tss_descriptor_t tss_desc;
    memset(&tss_desc, 0, sizeof(tss_descriptor_t));

    tss_desc.dpl = dpl;
    tss_desc.p = SEGMENT_PRESENT;
    tss_desc.type = IA32E_TSS;
    set_tss_desc_limit(&tss_desc, sizeof(tss_t) - 1);
    set_tss_desc_base(&tss_desc, (virt_addr_t) tss_segment);

    return tss_desc;
}

