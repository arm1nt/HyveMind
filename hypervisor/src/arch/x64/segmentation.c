#include "asm/segmentation.h"

extern void reload_cs_register(segment_selector_t cs_selector);

void
reload_segment_registers(const struct segment_regs *regs)
{
    reload_cs_register(regs->cs);
    asm volatile ("mov %0, %%ss" :: "m"(regs->ss));
}

