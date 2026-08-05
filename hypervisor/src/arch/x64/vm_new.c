#include "fatal.h"
#include "halloc.h"
#include "printf.h"
#include "vm.h"
#include "vmx/policy.h"
#include "vmx/vmx.h"

extern void vm_entry_test(void);

static inline void
init_default_virt_policy(struct vmx_virt_policy *policy)
{
    vmx_init_default_policy(policy);
}

static int
set_guest_info(vcpu_t *vcpu, struct vcpu_guest_arch_state *state)
{
    vcpu->arch.state.user_regs = state->uregs;

    vmx_set_guest_cr0(vcpu, state->cr0);
    vmx_set_guest_cr3(vcpu, state->cr3);
    vmx_set_guest_cr4(vcpu, state->cr4);

    vmx_set_segment_register(vcpu, X86_CS_REG, state->segments.cs);
    vmx_set_segment_register(vcpu, X86_SS_REG, state->segments.ss);
    vmx_set_segment_register(vcpu, X86_DS_REG, state->segments.ds);
    vmx_set_segment_register(vcpu, X86_ES_REG, state->segments.es);
    vmx_set_segment_register(vcpu, X86_FS_REG, state->segments.fs);
    vmx_set_segment_register(vcpu, X86_GS_REG, state->segments.gs);
    vmx_set_segment_register(vcpu, X86_TR_REG, state->segments.tr);
    vmx_set_segment_register(vcpu, X86_LDTR_REG, state->segments.ldtr);

    vmx_set_system_table(vcpu, X86_GDT, state->gdtr);
    vmx_set_system_table(vcpu, X86_IDT, state->idtr);

    return 0;
}

static int
init_vm_mirroring_vmm(struct vm *vm, const struct guest_config *config)
{
    int ret;
    vcpu_t *bsp, *ap;
    struct vmx_virt_policy *policy;
    struct vcpu_guest_arch_state vcpu_guest_state;

    bsp = vm->vcpus[0];
    policy = bsp->arch.hw.vmx.virt_policy;

    init_default_virt_policy(policy);
    policy->entry_ctls.ia32e_mode_guest = 1;
    policy->exit_ctls1.host_addr_space_size = 1;

    if ((ret = validate_vmx_virt_policy(policy)) != VMX_POLICY_VALID) {
        pr_error("Configured virt policy cannot be realized");
        return -1;
    }

    if ((ret = vmx_initialize_vmcs_area(bsp)) != 0) {
        pr_error("Failed to initialize vcpu's VMCS area: %lu", U64(ret));
        return ret;
    }

    vcpu_guest_mirror_current_cpu(&vcpu_guest_state);

    if ((ret = vcpu_guest_allocate_stack(&vcpu_guest_state, DEFAULT_VCPU_STACK_PAGES)) != 0) {
        pr_error("Failed to allocate stack for vcpu guest");
        return ret;
    }

    vcpu_guest_state.uregs.rip = __vaddr(vm_entry_test);

    if ((ret = set_guest_info(bsp, &vcpu_guest_state)) != 0) {
        pr_error("Failed to initialize the vcpu guest state");
        return ret;
    }

    /* remove */
    vmx_enter_vcpu(bsp);

    return 0;
}

static inline void
configure_linux_direct_boot_32bit_policy(struct vmx_virt_policy *policy)
{
    init_default_virt_policy(policy);

    policy->proc_ctls1.activate_secondary_controls = 1;
    policy->proc_ctls1.msr_bitmaps = 1;
    policy->proc_ctls2.enable_ept = 1;
    policy->proc_ctls2.unrestricted_guest = 1;

    policy->exit_ctls1.host_addr_space_size = 1;
    policy->exit_ctls1.load_ia32_efer = 1;
    policy->exit_ctls1.save_ia32_efer = 1;

    policy->entry_ctls.load_ia32_efer = 1;
}

static int
init_vm_linux_direct_boot_32bit(struct vm *vm, const struct guest_config *config)
{
    int ret;
    vcpu_t *bsp, *ap;
    struct vmx_virt_policy *policy;
    struct vcpu_guest_arch_state guest_state;

    bsp = vm->vcpus[0];
    policy = bsp->arch.hw.vmx.virt_policy;

    configure_linux_direct_boot_32bit_policy(policy);
    if ((ret = validate_vmx_virt_policy(policy)) != VMX_POLICY_VALID) {
        pr_error("Linux 32b VM setup configures an invalid virt policy: %le", U64(ret));
        return -1;
    }

    NOT_YET_IMPLEMENTED;
}

int
arch_init_vm(struct vm *vm, const struct guest_config *config)
{
    switch (config->guest_type) {
        case LINUX_DIRECT_BOOT_32BIT:
            return init_vm_linux_direct_boot_32bit(vm, config);
        case MIRROR_VMM:
            return init_vm_mirroring_vmm(vm, config);
        default:
            pr_error("Unsupported VM guest type configured");
            return -1;
    }

    die_reason("Unreachable");
}

void
destroy_arch_vm(struct vm *vm)
{
    /* De-allocate eptp structures, bitmaps, etc. */
}

int
allocate_arch_vcpu(struct vcpu *vcpu)
{
    int ret;

    vcpu->arch.hw.vmx.virt_policy = hmalloc(sizeof(struct vmx_virt_policy));
    if (!vcpu->arch.hw.vmx.virt_policy) {
        pr_error("Failed to allocate a vmx virt policy struct!");
        return -1;
    }

    ret = vmx_vcpu_allocate(vcpu);
    if (ret != 0) {
        pr_debug("Failed to allocate the vmcs area for the vcpu");
        hfree(vcpu->arch.hw.vmx.virt_policy);
        return ret;
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

    vmx_destroy_vcpu(vcpu);

    if (vcpu->arch.hw.vmx.virt_policy) {
        hfree(vcpu->arch.hw.vmx.virt_policy);
    }
}

