#include "fatal.h"
#include "mm_types.h"
#include "spinlock.h"
#include "asm/apic.h"
#include "asm/current.h"
#include "asm/gdt_idt.h"
#include "asm/irq.h"
#include "asm/irq_vectors.h"
#include "asm/idt.h"

#define NR_IRQ_VECTORS 256
/**
 * Since atm we only use the PIC for external interrupts, all IRQs will be
 * handled by the bsp, so a single table suffices
 */
static struct irq_desc irq_desc_table[NR_IRQ_VECTORS] = {
    [0 ... NR_IRQ_VECTORS-1] = {
        .used = false,
        .lock = DECLARE_SPINLOCK_RAW(),
    }
};

extern struct irq_domain legacy_pic_domain;

static inline void
execute_irq_handler_chain(struct irq_desc *desc)
{
    if (!desc->handler_chain)
        return;

    desc->handler_chain->handler(desc->irq_vector, desc->handler_chain->data);
}

/* irq_desc lock must be held before calling this function */
static inline void
__common_legacy_irq_trampoline(struct irq_desc *desc)
{
    if (desc->used) {
        execute_irq_handler_chain(desc);
    }

    desc->irq_domain->eoi(desc->device_irq);
}

void
common_legacy_irq_trampoline(const irq_t irq_vector)
{
    struct irq_desc *desc = &irq_desc_table[irq_vector];
    spinlock_acquire(&desc->lock);
    __common_legacy_irq_trampoline(desc);
    spinlock_release(&desc->lock);
}

void
common_legacy_irq_trampoline_spurious(const irq_t irq_vector)
{
    struct irq_desc *desc = &irq_desc_table[irq_vector];
    spinlock_acquire(&desc->lock);

    if (desc->irq_domain->is_spurious(irq_vector)) {
        pr_debug("Detected spurious IRQ on cpu (%lu): '%lu",
                get_current_cpuid(),
                irq_vector
        );
        desc->irq_domain->spurious_eoi(irq_vector);
        goto release_out;
    }

    __common_legacy_irq_trampoline(desc);
release_out:
    spinlock_release(&desc->lock);
}

static int
request_legacy_irq(struct irq_handler *handler, const hwirq_t device_irq)
{
    uint64_t flags;

    if (device_irq >= IRQ_LEGACY_VECTOR_NR) {
        pr_error("Invalid legacy IRQ line '%lu' requested!", device_irq);
        return -ERR_INVAL;
    }

    const irq_t irq_vec = IRQ_LEGACY_VECTOR(device_irq);
    struct irq_desc *desc = &irq_desc_table[irq_vec];
    spinlock_acquire_irq_save(&desc->lock, flags);

    if (desc->used || device_irq == 2) {
        /* For now, we only allow a single handler to be installed for a device_irq. */
        pr_error("IRQ line %lu (at irq vec %lu) is already occupied!", device_irq, irq_vec);
        spinlock_release_irq_restore(&desc->lock, flags);
        return -ERR_BUSY;
    }

    desc->used = true;
    desc->device_irq = device_irq;
    desc->irq_vector = irq_vec;
    desc->handler_chain = handler;

    desc->irq_domain->unmask_irq_line(device_irq);

    spinlock_release_irq_restore(&desc->lock, flags);
    return irq_vec;
}

int
request_irq(const enum irq_domain_type domain, const hwirq_t device_irq, struct irq_handler* handler)
{
    switch (domain) {
        case IRQ_DOMAIN_PIC:
            return request_legacy_irq(handler, device_irq);
        default:
            die_reason("Unsupported IRQ domain requested");
    }
}

int
remove_irq_handler(
        const enum irq_domain_type domain,
        const hwirq_t device_irq,
        const virt_addr_t handler_addr
) {
    /* Remove handler from the irq_desc using the handler struct addr as id */
    NOT_YET_IMPLEMENTED;
}

static inline void
init_legacy_irq_handling(void)
{
    legacy_pic_domain.init();

    /**
     * Add the legacy PIC trampolines to the IDT.
     *  When called, all PIC interrupts are masked and irqs on all processors
     *  are locally disabled too.
     */
    for (int i = 0; i < IRQ_LEGACY_VECTOR_NR; i++) {
        const irq_t irq_vector = IRQ_LEGACY_VECTOR(i);
        const virt_addr_t trampoline = __vaddr(legacy_irq_asm_trampolines[i]);
        REGISTER_INTERRUPT_GATE(&idt, irq_vector, trampoline, TSS_IST_NO_IST_STACK);

        irq_desc_table[irq_vector].irq_domain = &legacy_pic_domain;
        irq_desc_table[irq_vector].cpu_id = get_current_cpuid();
    }

    bsp_apic_enable_legacy_irq_support();
}

void
init_irqs(void)
{
    init_legacy_irq_handling();
}

void
enable_device_irqs(void)
{
    bsp_apic_enable_legacy_irqs();
}

