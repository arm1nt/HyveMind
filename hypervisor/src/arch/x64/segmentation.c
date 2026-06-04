#include "types.h"
#include "string.h"
#include "pf_alloc.h"
#include "asm/segmentation.h"
#include "asm/paging.h"

extern void reload_cs_register(segment_selector_t cs_selector);

void
reload_segment_registers(const struct segment_regs *regs)
{
    reload_cs_register(regs->cs);
    asm volatile ("mov %0, %%ss" :: "rm"(regs->ss));
}

inline void
reload_tr_register(const segment_selector_t *selector)
{
    asm volatile ("LTR %0" :: "m"(*selector));
}

int
init_default_tss(tss_t *tss, const unsigned int stack_size_pages)
{
    memset(tss, 0, sizeof(tss_t));

    virt_addr_t vaddr;
    if (get_pages_zeroed(stack_size_pages, &vaddr) != 0) {
        return -1;
    }

    /* Since SP grows downward */
    vaddr = align_down(vaddr + (stack_size_pages * PAGE_SIZE), 8);

    tss->ist1_low = U64_LOWER32(vaddr);
    tss->ist1_high = U64_UPPER32(vaddr);
    return 0;
}

void
set_tss_desc_limit(tss_descriptor_t *desc, const unsigned int limit)
{
    desc->limit0 = limit & ((1 << 16) - 1);
    desc->limit1 = (limit >> 16) & ((1 << 8) - 1);
}

void
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
create_tss_desc(const tss_t *tss_segment, const unsigned int dpl)
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

