#ifndef _HYVEMIND_X64_VM_VLAPIC_H
#define _HYVEMIND_X64_VM_VLAPIC_H

#include "mm_types.h"
#include "asm/arch_types.h"
#include "asm/apic_defs.h"

#define vcpu_vlapic(x) (&(x)->arch.hw.vmx.vlapic)

struct vm;
struct vcpu;

enum vlapic_mode {
    VLAPIC_DISABLED,
    VLAPIC_XAPIC,
    VLAPIC_X2APIC,
};

struct vlapic {
    enum vlapic_mode mode;

    lapicid_t physical_apic_id;
    apic_base_msr_t apic_base_msr;

    phys_addr_t virtual_apic_page;
    uint64_t timer_frequency;
};

enum vlapic_op_status {
    VLAPIC_SUCCESS,
    VLAPIC_ACCESS_PAGE_MAPPING_ERROR,
    VLAPIC_ERROR,
};

int remap_vlapic_base(struct vm *vm, const gpaddr vlapic_base);
void configure_vlapic_msr_intercepts(struct vm *vm);
int init_vlapic(struct vcpu *vcpu, const lapicid_t initial_apic_id, const bool is_bsp);

int guest_wrmsr_apic_base_msr(struct vcpu *vcpu, const uint64_t val);
int guest_apic_register_write(struct vcpu *vcpu, const int reg);

#endif /* _HYVEMIND_X64_VM_VLAPIC_H */

