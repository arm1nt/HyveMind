#ifndef _HYVEMIND_X64_VMX_VMX_OPS_H
#define _HYVEMIND_X64_VMX_VMX_OPS_H

#ifdef __ASSEMBLER__

.extern eflags_to_vmx_op_status

#else /* !__ASSEMBLER__ */

#include "fatal.h"
#include "vmx/vmcs.h"

#define VMX_OP_PREDEFINED_MIN_ERR_NR   1
#define VMX_OP_PREDEFINED_MAX_ERR_NR   28

enum vmx_op_status : int {
    VMX_OP_SUCCESS,

    /* Intel (pre-)defined error nrs */
    VMCALL_IN_VMX_ROOT                          = VMX_OP_PREDEFINED_MIN_ERR_NR,
    VMCLEAR_WITH_INVALID_PADDR,
    VMCLEAR_WITH_VMXON_PTR,
    VMLAUNCH_WITH_NONCLEAR_VMCS,
    VMRESUME_WITH_NONLAUNCHED_VMCS,
    VMRESUME_AFTER_VMXOFF,
    VM_ENTRY_INVALID_CTRL_FIELDS,
    VM_ENTRY_INVALID_HOST_STATE,
    VMPTRLD_WITH_INVALID_PADDR,
    VMPTRLD_WITH_VMXON_PTR,
    VMPTRLD_WITH_INVALID_VMCS_REVISION_ID,
    VMREAD_VMWRITE_TO_UNSUPPORTED_VMCS_COMPONENT,
    VMWMRITE_TO_RDONLY_VMCS_COMPONENT,
    VMXON_IN_VMX_ROOT                               = 15,
    VM_ENTRY_WITH_INVALID_EXECUTIVE_VMCS_PTR,
    VM_ENTRY_WITH_NONLAUNCHED_EXECUTIVE_VMCS,
    VM_ENTRY_WITH_EXECUTIVE_VMCS_PTR_NOT_VMXON_PTR,
    VMCALL_WITH_NONCLEAR_VMCS,
    VMCALL_WITH_INVALID_VM_EXIT_CTRL_FIELDS,
    VMCALL_WITH_INCORRECT_MSEG_REV_ID               = 22,
    VMXOFF_UNDER_DUAL_MONITOR,
    VMCALL_WITH_INVALID_SMM_FTRS,
    VM_ENTRY_INVALID_EX_CTRLS_IN_EXECUTIVE_VMCS,
    VM_ENTRY_WITH_EVENTS_BLOCKED_BY_MOV_SS,
    INVALID_OPERAND_TO_INVEPT_INVVPID           = VMX_OP_PREDEFINED_MAX_ERR_NR,

    /* Custom err nrs */
    VMX_OP_FAIL_INVALID,
    VMX_OP_UNKNOWN_ERROR,
};
typedef enum vmx_op_status vmx_op_status_t;

/**
 * TODO: Re-do error checking to ensure that we really see the rflags as left
 * by the vmx instruction. Also we can simplify the flag bit checks. Also
 * differentiate between eflags indicated state and err nr from the vmcs.
 */
static vmx_op_status_t __check_vm_op_status(const uint32_t flags);

#define query_vm_op_status() ({                            \
        uint64_t flags;                                    \
        asm volatile ("pushfq; pop %0" : "=r"(flags));     \
        __check_vm_op_status(flags);                       \
        })

static inline vmx_op_status_t
vmxon(const phys_addr_t vmxon_region)
{
    asm volatile("vmxon %0" :: "m"(vmxon_region) : "memory");
    return query_vm_op_status();
}

static inline vmx_op_status_t
vmxoff(void)
{
    asm volatile("vmxoff");
    return query_vm_op_status();
}

static inline vmx_op_status_t
vmclear(const phys_addr_t vmcs_area_ptr)
{
    asm volatile("vmclear %0" :: "m"(vmcs_area_ptr) : "memory");
    return query_vm_op_status();
}

static inline vmx_op_status_t
vmptrst(phys_addr_t *vmcs_addr_ptr)
{
    asm volatile("vmptrst %0" : "=m"(*vmcs_addr_ptr) :: "memory");
    return query_vm_op_status();
}

static inline vmx_op_status_t
vmptrld(const phys_addr_t vmcs_ptr)
{
    asm volatile("vmptrld %0" :: "m"(vmcs_ptr) : "memory");
    return query_vm_op_status();
}

/* In 64-bit mode, the destionation operand size is always 64 bit */
static inline vmx_op_status_t
vmread(const enum vmcs_field_encoding encoding, uint64_t *val)
{
    vmx_op_status_t res;
    asm volatile("vmread %1, %0" : "=m"(*val) : "r"(encoding) : "memory");
    res = query_vm_op_status();

#ifdef HYVEMIND_DEBUG_BUILD

    if (res != VMX_OP_SUCCESS) {
        pr_warn("'vmread' failed: %lu", res);
    }

#endif /* HYVEMIND_DEBUG_BUILD */

    return res;
}

static inline vmx_op_status_t
vmwrite(const enum vmcs_field_encoding encoding, const uint64_t value)
{
    vmx_op_status_t res;
    asm volatile("vmwrite %0, %1" :: "m"(value), "r"(encoding) : "memory");
    res = query_vm_op_status();

#ifdef HYVEMIND_DEBUG_BUILD

    if (res != VMX_OP_SUCCESS) {
        pr_warn("'vmwrite' failed: %lu", res);
    }

#endif /* HYVEMIND_DEBUG_BUILD */

    return res;
}

static inline bool
has_current_vmcs(void)
{
    vmx_op_status_t ret;
    phys_addr_t curr_vmcs_ptr;

    if ((ret = vmptrst(&curr_vmcs_ptr)) != VMX_OP_SUCCESS) {
        pr_warn("'has_current_vmcs()' failed to get current vmcs ptr: %lu", U64(ret));
        return false;
    }

    return curr_vmcs_ptr != NO_CURRENT_VMCS_ADDR;
}

static inline bool
__is_vm_succeed(const uint32_t eflags)
{
    return IS_CLEAR(
            eflags,
            (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_OF)
    );
}

static inline bool
__is_vm_fail_invalid(const uint32_t eflags)
{
    const bool set_res = IS_SET(eflags, EFLAGS_CF);
    const bool clear_res = IS_CLEAR(
            eflags,
            (EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_OF)
    );
    return set_res && clear_res;
}

static inline bool
__is_vm_fail_valid(const uint32_t eflags)
{
    if (!has_current_vmcs()) {
        return false;
    }

    const bool set_res = IS_SET(eflags, EFLAGS_ZF);
    const bool clear_res = IS_CLEAR(
            eflags,
            (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_SF | EFLAGS_OF)
    );
    return set_res && clear_res;
}

static inline enum vmx_op_status
__check_vm_op_status(const uint32_t flags)
{
    if (__is_vm_succeed(flags)) {
        return VMX_OP_SUCCESS;
    } else if (__is_vm_fail_invalid(flags)) {
        return VMX_OP_FAIL_INVALID;
    } else if (__is_vm_fail_valid(flags)) {
        uint64_t error_number;
        vmread(VM_INSTRUCTION_ERROR, &error_number);
        return error_number;
    } else {
        die_reason("Unreachable");
    }
}

static char *
stringify_vmx_op_status(const enum vmx_op_status status)
{
    switch (status) {
        case VMX_OP_SUCCESS:
            return "VMX_OP_SUCCESS";
        default:
            NOT_YET_IMPLEMENTED;
    }
}

/**
 * Must only be called for the current vcpu as otherwise we might read a
 * error number from the wrong vmcs.
 */
vmx_op_status_t eflags_to_vmx_op_status(const uint32_t flags);

#endif /* !__ASSEMBLER__ */

#endif /* _HYVEMIND_X64_VMX_VMX_OPS_H */

