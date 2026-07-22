#ifndef _HYVEMIND_X64_VMX_VMX_H
#define _HYVEMIND_X64_VMX_VMX_H

#include "mm_types.h"
#include "stdbool.h"
#include "vm.h"
#include "vmx/vmcs.h"

void tag_region_with_vmx_revisionid(phys_addr_t region);

bool enter_vmx_operation(void);

void leave_vmx_operation(void);

int init_vcpu(vcpu_t *vcpu);

/* Might return if vm entry fails */
void vmx_launch_vcpu(vcpu_t *vcpu);

void vmx_write_doubleword(vcpu_t *vcpu, const enum vmcs_field_encoding encoding, const uint32_t val);
void vmx_write_quadword(vcpu_t *vcpu, const enum vmcs_field_encoding encoding, const uint64_t val);

#endif /* _HYVEMIND_X64_VMX_VMX_H */

