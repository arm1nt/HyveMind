#include "fatal.h"
#include "string.h"
#include "per-cpu.h"
#include "printf.h"
#include "vmx/policy.h"

DEFINE_PER_CPU(vmx_capabilities_t, vmx_caps);

static inline vmcs_pin_ctls
set_reserved_pinbased_ctls(const vmcs_pin_ctls ctls)
{
    vmcs_pin_ctls sanitized;

    const uint64_t pinbased_msr = get_pinbased_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(pinbased_msr);
    const uint32_t allowed1 = U64_UPPER32(pinbased_msr);

    sanitized.raw = (ctls.raw | allowed0) & allowed1;
    return sanitized;
}

static inline vmcs_pin_ctls
set_reserved_pinbased_ctls_default1(vmcs_pin_ctls ctls)
{
    ctls.raw |= VMCS_PIN_BASED_CTLS_DEFAULT1;
    return set_reserved_pinbased_ctls(ctls);
}

static inline vmcs_procbased_ctls1
set_reserved_procbased_ctls1(const vmcs_procbased_ctls1 ctls)
{
    vmcs_procbased_ctls1 sanitized;

    const uint64_t procbased_msr = get_procbased_ctls1_msr();
    const uint32_t allowed0 = U64_LOWER32(procbased_msr);
    const uint32_t allowed1 = U64_UPPER32(procbased_msr);

    sanitized.raw = (ctls.raw | allowed0) & allowed1;
    return sanitized;
}

static inline vmcs_procbased_ctls1
set_reserved_procbased_ctls1_default1(vmcs_procbased_ctls1 ctls)
{
    ctls.raw |= VMCS_PROCBASED_CTLS1_DEFAULT1;
    return set_reserved_procbased_ctls1(ctls);
}

static inline vmcs_procbased_ctls2
set_reserved_procbased_ctls2(const vmcs_procbased_ctls2 ctls)
{
    vmcs_procbased_ctls2 sanitized;

    if (!vmx_caps_is_option_supported(VMX_POLICY_PROCBASED_CTLS2)) {
        sanitized.raw = 0;
        return sanitized;
    }

    const uint64_t procbased_msr = get_procbased_ctls2_msr();
    const uint32_t allowed1 = U64_UPPER32(procbased_msr);

    sanitized.raw = ctls.raw & allowed1;
    return sanitized;
}

static inline vmcs_procbased_ctls3
set_reserved_procbased_ctls3(const vmcs_procbased_ctls3 ctls)
{
    vmcs_procbased_ctls3 sanitized;

    if (!vmx_caps_is_option_supported(VMX_POLICY_PROCBASED_CTLS3)) {
        sanitized.raw = 0;
        return  sanitized;
    }

    const uint64_t procbased_msr = get_procbased_ctls3_msr();
    sanitized.raw = ctls.raw & procbased_msr;

    return sanitized;
}

static inline vmcs_exit_ctls1
set_reserved_vm_exit_ctls1(const vmcs_exit_ctls1 ctls)
{
    vmcs_exit_ctls1 sanitized;

    const uint64_t exit_msr = get_vmexit_ctls1_msr();
    const uint32_t allowed0 = U64_LOWER32(exit_msr);
    const uint32_t allowed1 = U64_UPPER32(exit_msr);

    sanitized.raw = (ctls.raw | allowed0) & allowed1;
    return  sanitized;
}

static inline vmcs_exit_ctls1
set_reserved_vm_exit_ctls1_default1(vmcs_exit_ctls1 ctls)
{
    ctls.raw |= VMCS_EXIT_CTLS1_DEFAULT1;
    return set_reserved_vm_exit_ctls1(ctls);
}

static inline vmcs_exit_ctls2
set_reserved_vm_exit_ctls2(const vmcs_exit_ctls2 ctls)
{
    vmcs_exit_ctls2 sanitized;

    if (!vmx_caps_is_option_supported(VMX_POLICY_VM_EXIT_CTLS2)) {
        sanitized.raw = 0;
        return sanitized;
    }

    const uint64_t exit2_msr = get_vmexit_ctls2_msr();
    sanitized.raw = ctls.raw & exit2_msr;
    return sanitized;
}

static inline vmcs_entry_ctls
set_reserved_vm_entry_ctls(const vmcs_entry_ctls ctls)
{
    vmcs_entry_ctls sanitized;

    const uint64_t entry_msr = get_vmentry_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(entry_msr);
    const uint32_t allowed1 = U64_UPPER32(entry_msr);

    sanitized.raw = (ctls.raw | allowed0) & allowed1;
    return sanitized;
}

static inline vmcs_entry_ctls
set_reserved_vm_entry_ctls_default1(vmcs_entry_ctls ctls)
{
    ctls.raw |= VMCS_ENTRY_CTLS_DEFAULT1;
    return set_reserved_vm_entry_ctls(ctls);
}

/**
 * Init capabilities to report options supported by the hypervisor and the system.
 * If a required option is not supported by the system, return a non-zero value.
 */
int
init_vmx_capabilities(void)
{
    vmcs_pin_ctls min_pin_ctls;
    min_pin_ctls.raw = 0;

    vmx_caps.pin_ctls = set_reserved_pinbased_ctls_default1(min_pin_ctls);
    if ((vmx_caps.pin_ctls.raw & min_pin_ctls.raw) != min_pin_ctls.raw) {
        pr_error("Pin controls do not support all required features!");
        return -1;
    }

    vmcs_procbased_ctls1 min_proc_ctls1;
    min_proc_ctls1.raw = 0;
    min_proc_ctls1.activate_secondary_controls = 1;
    min_proc_ctls1.msr_bitmaps = 1;

    vmx_caps.proc_ctls1 = set_reserved_procbased_ctls1_default1(min_proc_ctls1);
    if ((vmx_caps.proc_ctls1.raw & min_proc_ctls1.raw) != min_proc_ctls1.raw) {
        pr_error("Primary procbased controls do not support all required features!");
        return -1;
    }

    if (vmx_caps.proc_ctls1.activate_secondary_controls) {
        vmcs_procbased_ctls2 min_proc_ctls2;
        min_proc_ctls2.raw = 0;
        min_proc_ctls2.enable_ept = 1;
        min_proc_ctls2.unrestricted_guest = 1;

        vmx_caps.proc_ctls2 = set_reserved_procbased_ctls2(min_proc_ctls2);
        if ((vmx_caps.proc_ctls2.raw & min_proc_ctls2.raw) != min_proc_ctls2.raw) {
            pr_error("Secondary procbased ctls do not support all required features!");
            return -1;
        }

        if (vmx_caps.proc_ctls2.enable_ept) {
            /* TODO: Verify that the required EPT capabilities are supported */
        }
    } else {
        memset(&vmx_caps.proc_ctls2, 0, sizeof(vmx_caps.proc_ctls2));
    }

    if (vmx_caps.proc_ctls1.activate_tertiary_controls) {
        vmcs_procbased_ctls3 min_proc_ctls3;
        min_proc_ctls3.raw = 0;

        vmx_caps.proc_ctls3 = set_reserved_procbased_ctls3(min_proc_ctls3);
        if ((vmx_caps.proc_ctls3.raw & min_proc_ctls3.raw) != min_proc_ctls3.raw) {
            pr_error("Tertiary procbased ctls do not support all required features");
            return -1;
        }
    } else {
        memset(&vmx_caps.proc_ctls3, 0, sizeof(vmx_caps.proc_ctls3));
    }

    vmcs_exit_ctls1 min_exit_ctls1;
    min_exit_ctls1.raw = 0;
    min_exit_ctls1.ack_interrupt_on_exit = 1;
    min_exit_ctls1.host_addr_space_size = 1;
    min_exit_ctls1.load_ia32_efer = 1;
    min_exit_ctls1.save_ia32_efer = 1;

    vmx_caps.exit_ctls1 = set_reserved_vm_exit_ctls1_default1(min_exit_ctls1);
    if ((vmx_caps.exit_ctls1.raw & min_exit_ctls1.raw) != min_exit_ctls1.raw) {
        pr_error("Primary vm exit ctls do not support all required features");
        return -1;
    }

    if (vmx_caps.exit_ctls1.activate_secondary_controls) {
        vmcs_exit_ctls2 min_exit_ctls2;
        min_exit_ctls2.raw = 0;

        vmx_caps.exit_ctls2 = set_reserved_vm_exit_ctls2(min_exit_ctls2);
        if ((vmx_caps.exit_ctls2.raw & min_exit_ctls2.raw) != min_exit_ctls2.raw) {
            pr_error("Secondary vm exit ctls do not support all required features");
            return -1;
        }
    } else {
        memset(&vmx_caps.exit_ctls2, 0, sizeof(vmx_caps.exit_ctls2));
    }

    vmcs_entry_ctls min_entry_ctls;
    min_entry_ctls.raw = 0;
    min_entry_ctls.ia32e_mode_guest = 1;
    min_entry_ctls.load_ia32_efer = 1;

    vmx_caps.entry_ctls = set_reserved_vm_entry_ctls_default1(min_entry_ctls);
    if ((vmx_caps.entry_ctls.raw & min_entry_ctls.raw) != min_entry_ctls.raw) {
        pr_error("VM entry controls do not support all required features");
        return -1;
    }

    return 0;
}

/**
 * Basically a convenience function to check if a certain feature is theoretically
 * supported / enableable.
 */
bool
vmx_caps_is_option_supported(const enum vmx_policy_option option)
{
    switch (option) {
        case VMX_POLICY_PROCBASED_CTLS2:
            return vmx_caps.proc_ctls1.activate_secondary_controls;
        case VMX_POLICY_PROCBASED_CTLS3:
            return vmx_caps.proc_ctls1.activate_tertiary_controls;
        case VMX_POLICY_VM_EXIT_CTLS2:
            return vmx_caps.exit_ctls1.activate_secondary_controls;
        case VMX_POLICY_EPT:
            return vmx_caps.proc_ctls1.activate_secondary_controls
                && vmx_caps.proc_ctls2.enable_ept;
        case VMX_POLICY_MSR_BITMAP:
            return vmx_caps.proc_ctls1.msr_bitmaps;
        case VMX_POLICY_UNRESTRICTED_GUEST:
            return vmx_caps.proc_ctls1.activate_secondary_controls
                && vmx_caps.proc_ctls2.unrestricted_guest;
        default:
            return false;
    }

    die_reason("Unreachable");
}

void
vmx_init_default_policy(struct vmx_virt_policy *policy)
{
    memset(policy, 0, sizeof(struct vmx_virt_policy));

    policy->pin_ctls = set_reserved_pinbased_ctls_default1(policy->pin_ctls);
    policy->proc_ctls1 = set_reserved_procbased_ctls1_default1(policy->proc_ctls1);
    /* proc ctls 2&3 are per default initialized to all 0 */
    policy->exit_ctls1 = set_reserved_vm_exit_ctls1_default1(policy->exit_ctls1);
    /* vmexit ctls2 is per default initialized to all 0 */
    policy->entry_ctls = set_reserved_vm_entry_ctls_default1(policy->entry_ctls);
}

bool
vmx_policy_is_option_configured(
        const struct vmx_virt_policy *policy,
        const enum vmx_policy_option option
) {
    switch (option) {
        case VMX_POLICY_PROCBASED_CTLS2:
            return policy->proc_ctls1.activate_secondary_controls;
        case VMX_POLICY_PROCBASED_CTLS3:
            return policy->proc_ctls1.activate_tertiary_controls;
        case VMX_POLICY_VM_EXIT_CTLS2:
            return policy->exit_ctls1.activate_secondary_controls;
        case VMX_POLICY_EPT:
            return policy->proc_ctls1.activate_secondary_controls
                && policy->proc_ctls2.enable_ept;
        case VMX_POLICY_MSR_BITMAP:
            return policy->proc_ctls1.msr_bitmaps;
        case VMX_POLICY_UNRESTRICTED_GUEST:
            return policy->proc_ctls1.activate_secondary_controls
                && policy->proc_ctls2.unrestricted_guest;
        default:
            pr_warn("No policy-support lookup for: %lu", U64(option));
            return false;
    }

    die_reason("Unreachable");
}

#define VALIDATE_CONTROL(raw_caps, raw_policy) \
    (((raw_caps).raw & (raw_policy).raw) == (raw_policy).raw)

static int
policy_validates_against_capabilities(const struct vmx_virt_policy *policy)
{
    bool valid = false;

    valid = VALIDATE_CONTROL(vmx_caps.pin_ctls, policy->pin_ctls);
    if (!valid) {
        pr_error("Policy's pin controls are invalid");
        return VMX_POLICY_INVALID_OPTION_SET;
    }

    valid = VALIDATE_CONTROL(vmx_caps.proc_ctls1, policy->proc_ctls1);
    if (!valid) {
        pr_error("Policy's primary procbased ctls are invalid");
        return VMX_POLICY_INVALID_OPTION_SET;
    }

    if (policy->proc_ctls1.activate_secondary_controls) {
        valid = VALIDATE_CONTROL(vmx_caps.proc_ctls2, policy->proc_ctls2);
        if (!valid) {
            pr_error("Policy's secondary procbased ctls are invalid");
            return VMX_POLICY_INVALID_OPTION_SET;
        }
    }

    if (policy->proc_ctls1.activate_tertiary_controls) {
        valid = VALIDATE_CONTROL(vmx_caps.proc_ctls3, policy->proc_ctls3);
        if (!valid) {
            pr_error("Policy's tertiary procbased ctls are invalid");
            return VMX_POLICY_INVALID_OPTION_SET;
        }
    }

    valid = VALIDATE_CONTROL(vmx_caps.exit_ctls1, policy->exit_ctls1);
    if (!valid) {
        pr_error("Policy's primary vm exit controls are invalid");
        return VMX_POLICY_INVALID_OPTION_SET;
    }

    if (policy->exit_ctls1.activate_secondary_controls) {
        valid = VALIDATE_CONTROL(vmx_caps.exit_ctls2, policy->exit_ctls2);
        if (!valid) {
            pr_error("Policy's secondary vm exit ctls are invalid");
            return VMX_POLICY_INVALID_OPTION_SET;
        }
    }

    valid = VALIDATE_CONTROL(vmx_caps.entry_ctls, policy->entry_ctls);
    if (!valid) {
        pr_error("Policy's vm entry controls are invalid");
        return VMX_POLICY_INVALID_OPTION_SET;
    }

    return VMX_POLICY_VALID;
}

#undef VALIDATE_CONTROL

#define VALIDATE_RESERVED(ctls, func) ((ctls).raw == func((ctls)).raw)

static int
policy_validates_against_default_values(const struct vmx_virt_policy *policy)
{
    bool matches;

    matches = VALIDATE_RESERVED(policy->pin_ctls, set_reserved_pinbased_ctls);
    if (!matches) {
        pr_error("Policy erroneously configures reserved pin ctls");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->proc_ctls1, set_reserved_procbased_ctls1);
    if (!matches) {
        pr_error("Policy erroneously configures reserved proc ctls1 values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->proc_ctls2, set_reserved_procbased_ctls2);
    if (!matches) {
        pr_error("Policy erroneously configures reserved proc ctls2 values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->proc_ctls3, set_reserved_procbased_ctls3);
    if (!matches) {
        pr_error("Policy erroneously configures reserved proc ctls3 values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->exit_ctls1, set_reserved_vm_exit_ctls1);
    if (!matches) {
        pr_error("Policy erroneously configures reserved vm exit ctls1 values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->exit_ctls2, set_reserved_vm_exit_ctls2);
    if (!matches) {
        pr_error("Policy erroneously configures reserved vm exit ctls2 values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    matches = VALIDATE_RESERVED(policy->entry_ctls, set_reserved_vm_entry_ctls);
    if (!matches) {
        pr_error("Policy erroneously configures reserved vm entry ctls values");
        return VMX_POLICY_RESERVED_FIELD_CONFIGURED;
    }

    return VMX_POLICY_VALID;
}

#undef VALIDATE_RESERVED

/**
 * Based on the checks during a vm_entry.
 * Kinda a best effort approach to catch obvious issues and to make debugging
 * a bit simpler.
 */
static int
validate_control_dependencies(const struct vmx_virt_policy *policy)
{
    return 0;
    //NOT_YET_IMPLEMENTED;
}

int
validate_vmx_virt_policy(const struct vmx_virt_policy *policy)
{
    int ret;

    if ((ret = policy_validates_against_default_values(policy)) != VMX_POLICY_VALID) {
        pr_error("Policy erroneously configured reserved control values: %lu", U64(ret));
        return VMX_POLICY_INVALID;
    }

    if ((ret = policy_validates_against_capabilities(policy)) != VMX_POLICY_VALID) {
        pr_error("Policy does not validate against the supported capabilities: %lu", U64(ret));
        return VMX_POLICY_INVALID;
    }

    if(!policy->proc_ctls1.activate_secondary_controls && policy->proc_ctls2.raw) {
        pr_error("Policy's proc ctls2 are configured but not activated");
        return VMX_POLICY_INVALID;
    } else if (!policy->proc_ctls1.activate_tertiary_controls && policy->proc_ctls3.raw) {
        pr_error("Policy's proc ctls3 are configured but not activated");
        return VMX_POLICY_INVALID;
    } else if (!policy->exit_ctls1.activate_secondary_controls && policy->exit_ctls2.raw) {
        pr_error("Policy's vm exit ctls2 are configured but not activated");
        return  VMX_POLICY_INVALID;
    }

    if ((ret = validate_control_dependencies(policy)) != VMX_POLICY_VALID) {
        pr_error("Dependencies between options configured in the policy are not satisfied");
        return VMX_POLICY_INVALID;
    }

    return VMX_POLICY_VALID;
}

