#ifndef _HYVEMIND_X64_ASM_VM_H
#define _HYVEMIND_X64_ASM_VM_H

#include "guest_config.h"
#include "mm_types.h"
#include "asm/arch_types.h"
#include "asm/vcpu_guest_reg_state.h"
#include "asm/x86_defs.h"
#include "vmx/ept.h"
#include "vmx/vmcs.h"

struct vm;
struct vcpu;
struct vmx_virt_policy;

#define DEFAULT_VCPU_STACK_PAGES 10

enum vcpu_cpu_mode {
    VCPU_REAL_MODE,
    VCPU_PROTECTED_MODE,
    VCPU_LONG_MODE,
};

enum vcpu_run_status {
    VCPU_NOT_READY,
    VCPU_CREATED,
    VCPU_WAIT_FOR_SIPI,
    VCPU_RUNNABLE,
    VCPU_RUNNING,
    VCPU_HALTED,
    VCPU_STOPPED,
};

struct vmx_vcpu_state {
    enum vmcs_launch_state launch_state;
    phys_addr_t vmcs_ptr;

    /* todo: maybe move to vm struct instead of vcpu */
    struct vmx_virt_policy *virt_policy;
};

struct arch_vcpu {
    /* Is vcpu active on any cpu or not. Relevant for e.g. migrating vcpu */
    bool active;
    /* Logical processor on which the vcpu is active/current */
    cpuid_t active_processor;
    enum vcpu_run_status run_status;

    struct {
        cr0_t cr0;
        cr3_t cr3;
        cr4_t cr4;
        uint64_t dr7;
        ia32_efer_t efer;
        struct vcpu_user_regs user_regs;
    } state;

    struct {
        struct vmx_vcpu_state vmx;
    } hw;
};

struct arch_vm {
    struct {
        eptp_t eptp;
        void *msr_bitmap;
    } vmx;
};

int allocate_arch_vcpu(struct vcpu *vcpu);
void destroy_arch_vcpu(struct vcpu *vcpu);

int arch_init_vm(struct vm *vm, const struct guest_config *config);
void destroy_arch_vm(struct vm *vm);

#endif /* _HYVEMIND_X64_ASM_VM_H */

