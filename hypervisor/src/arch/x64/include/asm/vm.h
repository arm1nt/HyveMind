#ifndef _HYVEMIND_X64_ASM_VM_H
#define _HYVEMIND_X64_ASM_VM_H

#include "guest_config.h"
#include "asm/arch_types.h"
#include "vmx/vmcs.h"

struct arch_vcpu {
    /* Enclosing VM that this vcpu belongs to */
    void *vm;

    /* Logical processor on which the VMCS is active / current */
    cpuid_t active_processor;

    enum vmcs_launch_state launch_state;
    phys_addr_t vmcs_ptr;

    void *user_regs;
};
typedef struct arch_vcpu arch_vcpu_t;

struct arch_vm {};

void arch_create_vm_from_guest_config(const guest_cfg_t *config);

#endif /* _HYVEMIND_X64_ASM_VM_H */

