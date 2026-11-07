#ifndef _HYVEMIND_X64_VM_IRQ_H
#define _HYVEMIND_X64_VM_IRQ_H

#include "asm/irq.h"
#include "asm/irq_vectors.h"

#define VM_IRQ_LINE_UNMAPPED -1
typedef hwirq_t guest_hwirq_t;

struct vcpu;

bool vcpu_has_external_irq(struct vcpu *vcpu);
int vcpu_get_external_irq(struct vcpu *vcpu);

int vm_external_irq_handler(struct irq_desc *desc, void *data);

#endif /* _HYVEMIND_X64_VM_IRQ_H */

