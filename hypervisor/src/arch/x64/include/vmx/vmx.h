#ifndef _HYVEMIND_X64_VMX_VMX_H
#define _HYVEMIND_X64_VMX_VMX_H

#include "mm_types.h"
#include "vm.h"

void tag_region_with_vmx_revisionid(const phys_addr_t region);

bool enter_vmx_operation(void);
void leave_vmx_operation(void);

int vmx_vcpu_allocate(vcpu_t *vcpu);
void vmx_destroy_vcpu(vcpu_t *vcpu);

int vmx_initialize_vmcs_area(vcpu_t *vcpu);

int vmx_set_guest_cr0(vcpu_t *vcpu, const cr0_t);
void vmx_set_guest_cr3(vcpu_t *vcpu, const cr3_t);
void vmx_set_guest_cr4(vcpu_t *vcpu, const cr4_t);

int vmx_set_segment_register(
        vcpu_t *vcpu,
        const enum x86_segment_reg reg,
        const struct vcpu_segment segment
);

int vmx_set_system_table(
        vcpu_t *vcpu,
        const enum x86_sys_table type,
        struct vcpu_sys_table table
);

int vmx_enter_vcpu(vcpu_t *vcpu);

#endif /* _HYVEMIND_X64_VMX_VMX_H */

