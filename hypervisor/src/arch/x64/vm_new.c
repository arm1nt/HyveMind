#include "fatal.h"
#include "halloc.h"
#include "printf.h"
#include "vm.h"
#include "vmx/vmx.h"

int
init_bsp_arch_vcpu(vcpu_t *vcpu, const struct guest_config *config)
{
    switch (config->guest_type) {
        case LINUX_DIRECT_BOOT_32BIT:
            return -1;
        case MIRROR_VMM:
            return -1;
        default:
            pr_error("Unsupported/Unknown VM guest type configured");
            return -1;
    }

    die_reason("Unreachable");
}

int
arch_init_vm(struct vm *vm, const struct guest_config *config)
{
    int ret;

    for (unsigned int i = 0; i < vm->nr_vcpus; i++) {
        ret = init_vcpu(vm->vcpus[i]);

        if (ret != 0) {
            pr_error("Failed to initialize vcpu");
            return -1;
        }
    }

    ret = init_bsp_arch_vcpu(vm->vcpus[0], config);
    if (ret != 0) {
        pr_error("Failed to init BSP vcpu");
        return -1;
    }

    return 0;
}

int
allocate_arch_vcpu(struct vcpu *vcpu)
{
    pr_debug("Allocating arch vcpu members");

    vcpu->arch.hw.vmx.virt_policy = hmalloc(sizeof(struct vmx_virt_policy));
    if (!vcpu->arch.hw.vmx.virt_policy) {
        pr_error("Failed to allocate a virt policy struct");
        return -1;
    }

    return 0;
}

void
destroy_arch_vcpu(struct vcpu *vcpu)
{
    if (!vcpu) {
        return;
    }

    pr_debug("Destroying arch vcpu members");

    clear_vcpu(vcpu);

    if (vcpu->arch.hw.vmx.virt_policy) {
        hfree(vcpu->arch.hw.vmx.virt_policy);
    }
}

