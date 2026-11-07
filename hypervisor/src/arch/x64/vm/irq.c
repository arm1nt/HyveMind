#include "vm.h"
#include "vm/irq.h"
#include "asm/pic.h"

bool
vcpu_has_external_irq(struct vcpu *vcpu)
{
    if (!vlapic_accept_pic_irq(vcpu))
        return false;

    if (!vpic_has_pending_irq(vm_vpic(vcpu->vm)))
        return false;

    return true;
}

int
vcpu_get_external_irq(struct vcpu *vcpu)
{
    if (!vcpu_has_external_irq(vcpu))
        return -1;

    return vpic_get_pending_irq(vm_vpic(vcpu->vm));
}

int
vm_external_irq_handler(struct irq_desc *desc, void *data)
{
    struct vm *vm = (struct vm *) data;

    const hwirq_t host_irq = desc->device_irq;
    const guest_hwirq_t mapped_guest_irq = vm_devices(vm).host_to_guest_irq[host_irq];

    vpic_raise_external_irq(vm, mapped_guest_irq);
    return 0;
}

