#include "types.h"
#include "mm_types.h"
#include "string.h"
#include "printf.h"
#include "per-cpu.h"
#include "asm/gdt_idt.h"
#include "asm/idt.h"

DEFINE_PER_CPU_ALIGNED(struct gdt_struct, gdt_tables, PAGE_SIZE);
/* We share the idt across all logical processors */
static struct idt_struct idt;

void
__div_exception_handler(void)
{
    printf("Inside the c_div_exception_handler function");
}

void
__apic_oneshot_timer_handler(void)
{
    printf("apic oneshot timer fired");
}

static inline void
set_idt_gate_offset(idt_gate_t *gate, const uint64_t offset)
{
    const uint32_t lower32 = U64_LOWER32(offset);
    gate->low_offset = U32_LOWER16(lower32);
    gate->mid_offset = U32_UPPER16(lower32);
    gate->high_offset = U64_UPPER32(offset);
}

static idt_gate_t
create_idt_gate(const virt_addr_t handler_addr, const sys_seg_type_t type, const int ist_index)
{
    idt_gate_t gate;
    memset(&gate, 0, sizeof(gate));

    const segment_selector_t cs_selector = GDT_SELECTOR(HYVEMIND_CS_SEGMENT_INDEX, TI_GDT, 0);
    gate.selector = cs_selector;

    set_idt_gate_offset(&gate, handler_addr);
    gate.idt_bits.p = SEGMENT_PRESENT;
    gate.idt_bits.type = type;
    gate.idt_bits.ist = ist_index;

    return gate;
}

static inline void
write_idt_entry(struct idt_struct *idt, const int index, const idt_gate_t gate)
{
    memcpy(((uint8_t *)idt) + (index * 16), &gate, sizeof(idt_gate_t));
}

static inline void
register_idt_handler(
        struct idt_struct *idt,
        const int index,
        const sys_seg_type_t type,
        const virt_addr_t handler,
        const int ist_index
)
{
    const idt_gate_t gate = create_idt_gate(handler, type, ist_index);
    write_idt_entry(idt, index, gate);
}

#define REGISTER_INTERRUPT_GATE(idt, index, handler, ist_index) \
    register_idt_handler(idt, index, IA32E_INTERRUPT_GATE, (virt_addr_t)handler, ist_index)

#define REGISTER_TRAP_GATE(idt, index, handler, ist_index)      \
    register_idt_handler(idt, index, IA32E_TRAP_GATE, (virt_addr_t)handler, ist_index)

static inline void
_load_idt(const struct idt_struct *idt)
{
    const idt_ptr_t install_ptr = {
        .limit = IDT_LIMIT(IDT_ENTRIES),
        .base = (uint64_t) idt
    };

    asm volatile ("LIDT %0" :: "m"(install_ptr));
}

void
init_shared_idt(void)
{
    memset(&idt, 0, sizeof(struct idt_struct));

    REGISTER_TRAP_GATE(&idt, IRQ_DIVIDE_ERROR_VECTOR, asm_div_exception_handler, 0);
    REGISTER_INTERRUPT_GATE(&idt, APIC_ONESHOT_TIMER_VECTOR, asm_irq_apic_oneshot_timer_handler, TSS_IST_INDEX1);
}

void
load_shared_idt(void)
{
    _load_idt(&idt);
}

static inline void
write_gdt_entry(
        struct gdt_struct *gdt,
        const void *desc,
        const unsigned int index,
        const desc_type_t type
)
{
    unsigned int size;

    if (type == SYSTEM_SEGMENT_DESC) {
        size = 16;
    } else {
        size = 8;
    }

    memcpy(&gdt->gdt[index], desc, size);
}

static inline void
_load_gdt(const struct gdt_struct *gdt)
{
    const gdt_ptr_t install_ptr = {
        .limit = GDT_LIMIT(GDT_ENTRIES),
        .base = (uint64_t) gdt
    };

    asm volatile ("LGDT %0" :: "m"(install_ptr));
}

void
init_new_gdt(void)
{
    struct gdt_struct *gdt_table = percpu_ptr(gdt_tables);

    memset(gdt_table, 0, sizeof(*gdt_table));
    write_gdt_entry(gdt_table, &hyvemind_cs_segment_desc, HYVEMIND_CS_SEGMENT_INDEX, CODE_DATA_SEGMENT_DESC);
    write_gdt_entry(gdt_table, &hyvemind_data_segment_desc, HYVEMIND_DATA_SEGMENT_INDEX, CODE_DATA_SEGMENT_DESC);
}

int
install_new_tss(void)
{
    struct gdt_struct *gdt = percpu_ptr(gdt_tables);

    tss_t tss_segment;
    if (init_default_tss(&tss_segment, TSS_IST_DEFAULT_SIZE_PAGES) != 0)  {
        pr_error("Failed to create a new TSS segment");
        return -1;
    }
    tss_descriptor_t tss_desc = create_tss_desc(&tss_segment, 0);

    write_gdt_entry(gdt, &tss_desc, HYVEMIND_TSS_INDEX, SYSTEM_SEGMENT_DESC);
    return 0;
}

void
load_gdt(void)
{
    _load_gdt(percpu_ptr(gdt_tables));

    const struct segment_regs regs = {
        .cs = GDT_SELECTOR(HYVEMIND_CS_SEGMENT_INDEX, TI_GDT, 0),
        .ss = GDT_SELECTOR(HYVEMIND_DATA_SEGMENT_INDEX, TI_GDT, 0),
        .ds = GDT_NULL_SELECTOR,
        .es = GDT_NULL_SELECTOR,
        .fs = GDT_NULL_SELECTOR,
        .gs = GDT_NULL_SELECTOR
    };
    reload_segment_registers(&regs);

    segment_selector_t tss_selector = GDT_SELECTOR(HYVEMIND_TSS_INDEX, TI_GDT, 0);
    reload_tr_register(&tss_selector);
}

