#ifndef _HYVEMIND_X64_ASM_VM_H
#define _HYVEMIND_X64_ASM_VM_H

#include "guest_config.h"
#include "mm_types.h"
#include "asm/arch_types.h"
#include "asm/vm_arch_state.h"
#include "asm/x86_defs.h"
#include "vmx/vmcs.h"

struct vm;
struct vcpu;
struct vmx_virt_policy;

struct vmx_vcpu_state {
    enum vmcs_launch_state launch_state;
    phys_addr_t vmcs_ptr;

    struct vmx_virt_policy *virt_policy;
};

struct arch_vcpu {
    /* Logical processor on which the vcpu is active/current */
    cpuid_t active_processor;

    struct {
        enum vcpu_cpu_mode cpu_mode;

        cr0_t cr0;
        uint64_t cr3; /* change */
        cr4_t cr4;
        uint64_t dr7;

        ia32_efer_t efer;

        struct vcpu_user_regs user_regs;

        struct vcpu_segments segments;
        struct vcpu_sys_table gdtr;
        struct vcpu_sys_table idtr;
    } state;

    struct {
        struct vmx_vcpu_state vmx;
    } hw;
};

int allocate_arch_vcpu(struct arch_vcpu *vcpu);
void destroy_arch_vcpu(struct arch_vcpu *vcpu);

int arch_init_vm(struct vm *vm, const struct guest_config *config);

#endif /* _HYVEMIND_X64_ASM_VM_H */

