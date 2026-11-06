#ifndef _HYVEMIND_X64_VMX_VMX_OPS_H
#define _HYVEMIND_X64_VMX_VMX_OPS_H

#include "fatal.h"
#include "mm_types.h"
#include "printf.h"
#include "vmx/vmcs.h"
#include "vmx/vmx_defs.h"

#define VMX_OP_NO_OUTPUTS
#define VMX_OP_NO_INPUTS
#define VMX_OP_NO_CLOBBERS
#define VMX_OP_MULTIPLE_ARGS(arg1, arg2) arg1, arg2

static void __raw_vmread(const vmcs_field_encoding_t field, uint64_t *val);

#define __asm_do_vmx_op(cmd, outputs, inputs, clobbers) ({      \
        int __ret = VMX_SUCCESS;                                \
                                                                \
        asm goto(cmd ";"                                        \
                "jc %l[__vmfail_invalid];"                      \
                "je %l[__vmfail_valid];"                        \
                "ja %l[__vmsucceed];"                           \
                : outputs                                       \
                : inputs                                        \
                : clobbers                                      \
                : __vmfail_invalid, __vmfail_valid, __vmsucceed \
        );                                                      \
        die_reason("unreachable");                              \
                                                                \
        __vmfail_invalid:                                       \
            __ret = VMX_OP_FAIL_INVALID;                        \
            goto __out;                                         \
        __vmfail_valid:                                         \
            uint64_t __err_nr;                                  \
            __raw_vmread(VM_INSTRUCTION_ERROR, &__err_nr);      \
            __ret = __err_nr;                                   \
            goto __out;                                         \
        __vmsucceed:                                            \
        __out:                                                  \
            __ret;                                              \
        })

static inline int
vmxon(const phys_addr_t vmxon_region_ptr)
{
    return __asm_do_vmx_op("vmxon %0", VMX_OP_NO_OUTPUTS, "m"(vmxon_region_ptr), "memory");
}

static inline int
vmxoff(void)
{
    return __asm_do_vmx_op("vmxoff", VMX_OP_NO_OUTPUTS, VMX_OP_NO_INPUTS, VMX_OP_NO_CLOBBERS);
}

static inline int
vmclear(const phys_addr_t vmcs_ptr)
{
    return __asm_do_vmx_op("vmclear %0", VMX_OP_NO_OUTPUTS, "m"(vmcs_ptr), "memory");
}

static inline int
vmptrst(phys_addr_t *vmcs_addr_ptr)
{
    return __asm_do_vmx_op("vmptrst %0", "=m"(*vmcs_addr_ptr), VMX_OP_NO_INPUTS, "memory");
}

static inline int
vmptrld(const phys_addr_t vmcs_ptr)
{
    return __asm_do_vmx_op("vmptrld %0", VMX_OP_NO_OUTPUTS, "m"(vmcs_ptr), "memory");
}

static inline void
__raw_vmread(const vmcs_field_encoding_t field, uint64_t *val)
{
    asm volatile("vmread %1, %0" : "=m"(*val) : "r"(field) : "memory");
}

/* In 64-bit mode, the destionation operand size is always 64 bit */
static inline int
vmread(const enum vmcs_field_encoding encoding, uint64_t *val)
{
    const int res = __asm_do_vmx_op("vmread %1, %0", "=m"(*val), "r"(encoding), "memory");

#ifdef HYVEMIND_DEBUG_BUILD
    if (res != VMX_SUCCESS)
        pr_warn("vmread failed for field '%lu'", encoding);
#endif
    return res;
}

static inline int
vmwrite(const enum vmcs_field_encoding encoding, const uint64_t value)
{
    const int res = __asm_do_vmx_op(
            "vmwrite %0, %1",
            VMX_OP_NO_OUTPUTS,
            VMX_OP_MULTIPLE_ARGS("m"(value), "r"(encoding)),
            "memory"
    );

#ifdef HYVEMIND_DEBUG_BUILD
    if (res != VMX_SUCCESS)
        pr_warn("vmwrite failed for field '%lu'", encoding);
#endif
    return res;
}

#endif /* _HYVEMIND_X64_VMX_VMX_OPS_H */

