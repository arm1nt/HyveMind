#include "gdt_idt.h"
#include "string.h"

extern void reload_cs_register(segment_selector_t cs_selector);

static struct gdt_struct gdt;
static struct idt_struct idt;

static void __attribute__((noinline))
reload_segment_registers(const struct segment_regs *regs)
{
    reload_cs_register(regs->cs);
    asm volatile ("mov %0, %%ss" :: "m"(regs->ss));
}

static inline void
load_gdt(const struct gdt_struct *gdt)
{
    const gdt_ptr_t install_ptr = {
        .limit = GDT_LIMIT(GDT_ENTRIES),
        .base = (uint64_t) gdt
    };

    asm volatile ("LGDT %0" :: "m"(install_ptr));
}

static void
setup_gdt(void)
{
    memset(&gdt, 0, sizeof(struct gdt_struct));
    gdt.gdt[HYVEMIND_CS_SEGMENT_INDEX] = hyvemind_cs_segment_desc;
    gdt.gdt[HYVEMIND_DATA_SEGMENT_INDEX] = hyvemind_data_segment_desc;

    load_gdt(&gdt);

    const struct segment_regs regs = {
        .cs = hyvemind_cs_selector,
        .ss = hyvemind_data_selector,
    };

    reload_segment_registers(&regs);
}

int
setup_gdt_idt(void)
{
    setup_gdt();

    return 0;
}

