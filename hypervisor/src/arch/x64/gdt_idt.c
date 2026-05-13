#include "gdt_idt.h"
#include "idt.h"
#include "string.h"
#include "printf.h"

extern void reload_cs_register(segment_selector_t cs_selector);

static struct gdt_struct gdt;
static struct idt_struct idt;

void
c_div_exception_handler(void)
{
    printf("Inside the c_div_exception_handler function");
}

void
c_double_fault_handler(void)
{
    printf("Inside the c_double_fault_handler function");
}

static idt_gate_t
generate_idt_gate(const uint64_t handler_addr)
{
    idt_gate_t gate;

    gate.selector = hyvemind_cs_selector;
    set_idt_gate_offset(&gate, handler_addr);
    gate.idt_bits = INTERRUPT_IDT_BITS;
    gate.reserved = U32(0);

    return gate;
}

static inline void
register_idt_handler(struct idt_struct *idt, const int index, const idt_gate_t gate)
{
    memcpy(&idt->gates[index], &gate, sizeof(idt_gate_t));
}

#define REGISTER_INTERRUPT_HANDLER(index, handler) \
    register_idt_handler(&idt, index, generate_idt_gate((uint64_t) handler))

static inline void
load_idt(const struct idt_struct *idt)
{
    const idt_ptr_t install_ptr = {
        .limit = IDT_LIMIT(IDT_ENTRIES),
        .base = (uint64_t) idt
    };

    asm volatile ("LIDT %0" :: "m"(install_ptr));
}

static void
setup_idt(void)
{
    memset(&idt, 0, sizeof(struct idt_struct));

    REGISTER_INTERRUPT_HANDLER(DIVIDE_ERROR_VECTOR, asm_div_exception_handler);
    REGISTER_INTERRUPT_HANDLER(DOUBLE_FAULT_VECTOR, asm_double_fault_handler);

    load_idt(&idt);
}

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

int inline
setup_gdt_idt(void)
{
    setup_gdt();
    setup_idt();
    return 0;
}

