#ifndef _HYVEMIND_X64_VMX_POLICY_H
#define _HYVEMIND_X64_VMX_POLICY_H

#include "vmx/vmcs.h"

struct vmx_virt_policy {
    vmcs_pin_ctls pin_ctls;

    vmcs_procbased_ctls1 proc_ctls1;
    vmcs_procbased_ctls2 proc_ctls2;
    vmcs_procbased_ctls3 proc_ctls3;

    vmcs_exit_ctls1 exit_ctls1;
    vmcs_exit_ctls2 exit_ctls2;

    vmcs_entry_ctls entry_ctls;
};

/**
 * Conceptually different from a policy. Here we set the default ctl values and
 * set every feature that can be enabled that is supported by both the hypervisor
 * and the system.
 */
typedef struct vmx_virt_policy vmx_capabilities_t;

/**
 * For now a percpu var as simplification. Perhaps later replace with a single
 * global struct that represents the intersection of supported features across
 * all processors we run on.
 */
DECLARE_PER_CPU(vmx_capabilities_t, vmx_caps);
#define vmx_caps (percpu_val(vmx_caps))

enum vmx_policy_option {
    VMX_POLICY_PROCBASED_CTLS2,
    VMX_POLICY_PROCBASED_CTLS3,
    VMX_POLICY_VM_EXIT_CTLS2,
    VMX_POLICY_EPT,
};

enum vmx_policy_vrfy_status {
    VMX_POLICY_VALID,
    VMX_POLICY_INVALID,
    VMX_POLICY_RESERVED_FIELD_CONFIGURED,
    VMX_POLICY_INVALID_OPTION_SET,
    /* E.g. if procbased_ctls2 is activated even though its not supported */
    VMX_POLICY_UNSUPPORTED_CTL_ACTIVATED,
};

int init_vmx_capabilities(void);

bool vmx_caps_is_option_supported(const enum vmx_policy_option option);

void vmx_init_default_policy(struct vmx_virt_policy *policy);

bool vmx_policy_is_option_configured(
        const struct vmx_virt_policy *policy,
        const enum vmx_policy_option option
);

int validate_vmx_virt_policy(const struct vmx_virt_policy *policy);

#endif /* _HYVEMIND_X64_VMX_POLICY_H */

