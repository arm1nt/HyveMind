#ifndef _HYVEMIND_X64_VMX_VMX_H
#define _HYVEMIND_X64_VMX_VMX_H

#include "mm_types.h"
#include "vm.h"

void tag_region_with_vmx_revisionid(const phys_addr_t region);

cr0_t sanitize_cr0_for_vmx_operation(const cr0_t cr0);
cr3_t sanitize_cr3_for_vmx_operation(cr3_t cr3);
cr4_t sanitize_cr4_for_vmx_operation(const cr4_t cr4);

bool enter_vmx_operation(void);
void leave_vmx_operation(void);

int vmx_vcpu_allocate(vcpu_t *vcpu);
void vmx_destroy_vcpu(vcpu_t *vcpu);

int vmx_initialize_vmcs_area(vcpu_t *vcpu);

int vmx_set_guest_cr0(vcpu_t *vcpu, const cr0_t cr0);
void vmx_set_guest_cr3(vcpu_t *vcpu, const cr3_t cr3);
void vmx_set_guest_cr4(vcpu_t *vcpu, const cr4_t cr4);

void vmx_get_default_cr4_mask_and_shadow(
        uint64_t *default_mask,
        uint64_t *default_shadow
);
void vmx_set_cr4_mask_and_shadow(vcpu_t *vcpu, const uint64_t mask, const uint64_t shadow);

void vmx_set_guest_efer(vcpu_t *vcpu, const ia32_efer_t efer);

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

void vmx_inject_exception(vcpu_t *vcpu, const int vector, const int error_code);

int vmx_enter_vcpu(vcpu_t *vcpu);

#endif /* _HYVEMIND_X64_VMX_VMX_H */

