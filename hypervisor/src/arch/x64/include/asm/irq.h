#ifndef _HYVEMIND_X64_ASM_IRQ_H
#define _HYVEMIND_X64_ASM_IRQ_H

#include "hyvstdlib.h"
#include "mm_types.h"
#include "spinlock.h"
#include "asm/arch_types.h"

/* IRQ line raised by a device */
typedef int hwirq_t;
/* IDT vector nr */
typedef uint8_t irq_t;

enum irq_domain_type {
    IRQ_DOMAIN_PIC,
    IRQ_DOMAIN_IOAPIC,
};

struct irq_domain {
    void (*init)(void);
    bool (*is_spurious)(const irq_t irq);
    void (*spurious_eoi)(const irq_t irq);
    void (*eoi)(const irq_t irq_vector);
    void (*unmask_irq_line)(const hwirq_t device_irq);
    void (*mask_irq_line)(const hwirq_t device_irq);
};

struct irq_handler {
    void *data;
    int (*handler)(irq_t irq_vector, void *data);
};

struct irq_desc {
    bool used;
    spinlock_t lock;

    cpuid_t cpu_id;
    hwirq_t device_irq;
    irq_t irq_vector;

    struct irq_domain *__borrowed irq_domain;
    struct irq_handler *__borrowed handler_chain;
};

void init_irqs(void);
void enable_irqs(void);
int request_irq(const enum irq_domain_type domain, const hwirq_t device_irq, struct irq_handler* handler);
int remove_irq_handler(const enum irq_domain_type domain, const hwirq_t device_irq, const virt_addr_t handler_addr);

#endif /* _HYVEMIND_X64_ASM_IRQ_H */

