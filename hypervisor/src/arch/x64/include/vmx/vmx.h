#ifndef _HYVEMIND_X64_VMX_VMX_H
#define _HYVEMIND_X64_VMX_VMX_H

#include "mm_types.h"
#include <stdbool.h>
#include "vm.h"

void tag_region_with_vmx_revisionid(phys_addr_t region);

bool enter_vmx_operation(void);

void leave_vmx_operation(void);

int init_vcpu(vcpu_t *vcpu);

#endif /* _HYVEMIND_X64_VMX_VMX_H */

