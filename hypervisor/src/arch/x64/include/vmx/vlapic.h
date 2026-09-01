#ifndef _HYVEMIND_X64_VMX_VLAPIC_H
#define _HYVEMIND_X64_VMX_VLAPIC_H

#include "mm_types.h"
#include "asm/apic_defs.h"

#define vcpu_vlapic(x) (&(x)->arch.hw.vmx.vlapic)

struct vcpu;

enum vlapic_mode {
    VLAPIC_DISABLED,
    VLAPIC_XAPIC,
    VLAPIC_X2APIC,
};

struct vlapic {
    enum vlapic_mode mode;

    uint32_t initial_apic_id;
    apic_base_msr_t apic_base_msr;

    phys_addr_t virtual_apic_page;
};

enum vlapic_op_status {
    VLAPIC_SUCCESS,
    VLAPIC_ERROR,
};

uint64_t get_vlapic_mem_base(const struct vlapic *vlapic);
int guest_wrmsr_apic_base_msr(struct vcpu *vcpu, const uint64_t val);

int init_vlapic(struct vcpu *vcpu, const uint32_t initial_apic_id);

#endif /* _HYVEMIND_X64_VMX_VLAPIC_H */


