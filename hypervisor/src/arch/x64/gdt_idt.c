#include "types.h"
#include "gdt_idt.h"
#include "idt.h"
#include "mm_types.h"
#include "string.h"
#include "printf.h"

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
    memcpy(&idt->gates[index], &gate, sizeof(idt_gate_t));
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

    REGISTER_TRAP_GATE(&idt, DIVIDE_ERROR_VECTOR, asm_div_exception_handler, 0);
    REGISTER_TRAP_GATE(&idt, DOUBLE_FAULT_VECTOR, asm_double_fault_handler, 1);

    load_idt(&idt);
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
load_gdt(const struct gdt_struct *gdt)
{
    const gdt_ptr_t install_ptr = {
        .limit = GDT_LIMIT(GDT_ENTRIES),
        .base = (uint64_t) gdt
    };

    asm volatile ("LGDT %0" :: "m"(install_ptr));
}

static int
setup_gdt(void)
{
    memset(&gdt, 0, sizeof(struct gdt_struct));
    write_gdt_entry(&gdt, &hyvemind_cs_segment_desc, HYVEMIND_CS_SEGMENT_INDEX, CODE_DATA_SEGMENT_DESC);
    write_gdt_entry(&gdt, &hyvemind_data_segment_desc, HYVEMIND_DATA_SEGMENT_INDEX, CODE_DATA_SEGMENT_DESC);

    tss_t tss_segment;
    if (init_default_tss(&tss_segment, TSS_IST_DEFAULT_SIZE_PAGES) != 0)  {
        return -1;
    }
    init_default_tss(&tss_segment, TSS_IST_DEFAULT_SIZE_PAGES);
    tss_descriptor_t tss_desc = create_tss_desc(&tss_segment, 0);
    write_gdt_entry(&gdt, &tss_desc, HYVEMIND_TSS_INDEX, SYSTEM_SEGMENT_DESC);

    load_gdt(&gdt);

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

    return 0;
}

int
setup_gdt_idt(void)
{
    int ret = 0;

    ret = setup_gdt();
    if (ret) {
        return ret;
    }

    setup_idt();

    return 0;
}

