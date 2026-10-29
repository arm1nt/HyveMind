#ifndef _HYVEMIND_X64_ASM_VM_H
#define _HYVEMIND_X64_ASM_VM_H

#include "mm_types.h"
#include "spinlock.h"
#include "vm_config.h"
#include "asm/arch_types.h"
#include "asm/vcpu_guest_reg_state.h"
#include "asm/x86_defs.h"
#include "vm/vlapic.h"
#include "vm/vpic.h"
#include "vmx/ept_types.h"
#include "vmx/emulate.h"
#include "vmx/vmcs.h"

struct vm;
struct vcpu;
struct vmx_virt_policy;

#define DEFAULT_VCPU_STACK_PAGES 10

struct vmx_vcpu_state {
    phys_addr_t vmcs_ptr;

    cpuid_t active_cpu;
    enum vmcs_launch_state launch_state;

    struct vlapic vlapic;
    struct vmx_virt_policy *virt_policy;
};

#define vcpu_vmx(vcpu) ((vcpu)->arch.hw.vmx)
#define vcpu_event_lock(vcpu) (&(vcpu)->arch.event_queue_lock)

struct arch_vcpu {
    bool is_bsp;

    spinlock_t event_queue_lock;

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

#define vm_vmx_state(x)         ((x)->arch_vm.vmx)
#define vm_io_bitmap_addr(x)    (vm_vmx_state(x).io_bitmap_addr)
#define vm_msr_bitmap_addr(x)   (vm_vmx_state(x).msr_bitmap_addr)
#define vm_apic_access_page(x)  (vm_vmx_state(x).apic_access_page)

struct arch_vm {
    struct vpic *vpic;

    struct {
        int host_serial_pio_base;
        /* Map guest IRQ line to corresponding host IRQ line and vice versa */
        int host_to_guest_irq[256];
        int guest_to_host_irq[256];
    } devices;

    struct {
        eptp_t eptp;
        phys_addr_t io_bitmap_addr;
        phys_addr_t msr_bitmap_addr;
        phys_addr_t apic_access_page;
        struct x86_emulate_ops ops;
    } vmx;
};

int arch_alloc_vcpu(struct vcpu *vcpu);
void arch_destroy_vcpu(struct vcpu *vcpu);
int arch_prepare_execution_env(struct vm *vm, const struct vm_config *config);

#endif /* _HYVEMIND_X64_ASM_VM_H */

