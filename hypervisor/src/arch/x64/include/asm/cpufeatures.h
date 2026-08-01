#ifndef _HYVEMIND_X64_ASM_CPUFEATURES_H
#define _HYVEMIND_X64_ASM_CPUFEATURES_H

#include "types.h"

/******************************************************************************
 * CPUID related constants
******************************************************************************/

#define CPUID_RANGE_BASE_VAL                    0x00
#define CPUID_EXT_RANGE_BASE_VAL                0x80000000

#define CPUID_BASE_RANGE_LIMITS_LEAF            0x00
#define CPUID_EXTENDED_RANGE_LIMITS_LEAF        0x80000000
#define CPUID_BRAND_STRING_LEAF                 0x00
#define CPUID_CPU_FEATURES_LEAF                 0x01
#define CPUID_PAGE_1GB_LEAF                     0x80000001
#define CPUID_MAX_PHYS_ADDR_LEAF                0x80000008
#define CPUID_MAX_APIC_ADDR_LEAF                0x80000008

#define CPUID_VMX_BIT                           5 /* cpuid_result.ecx */
#define CPUID_VMX                               (U32(1) << CPUID_VMX_BIT)
#define CPUID_MSR_BIT                           5 /* cpuid_result.edx */
#define CPUID_MSR                               (U32(1) << CPUID_MSR_BIT)
#define CPUID_LAPIC_BIT                         9 /* cpuid_result.edx */
#define CPUID_LAPIC                             (U32(1) << CPUID_LAPIC_BIT)
#define CPUID_X2APIC_MODE_BIT                   21 /* cpuid_result.ecx */
#define CPUID_X2APIC_MODE                       (U32(1) << CPUID_X2APIC_MODE_BIT)
#define CPUID_PAGE_1GB_BIT                      26 /* cpuid_result.eax */
#define CPUID_PAGE_1GB                          (U32(1) << CPUID_PAGE_1GB_BIT)

/******************************************************************************
 * EFLAGS related constants
******************************************************************************/

#define EFLAGS_CF_BIT                           0 /* carry flag */
#define EFLAGS_CF                               U64_LSHIFT(1, EFLAGS_CF_BIT)
#define EFLAGS_PF_BIT                           2 /* parity flag */
#define EFLAGS_PF                               U64_LSHIFT(1, EFLAGS_PF_BIT)
#define EFLAGS_AF_BIT                           4 /* auxilliary carry flag */
#define EFLAGS_AF                               U64_LSHIFT(1, EFLAGS_AF_BIT)
#define EFLAGS_ZF_BIT                           6 /* zero flag */
#define EFLAGS_ZF                               U64_LSHIFT(1, EFLAGS_ZF_BIT)
#define EFLAGS_SF_BIT                           7 /* sign flag */
#define EFLAGS_SF                               U64_LSHIFT(1, EFLAGS_SF_BIT)
#define EFLAGS_OF_BIT                           11 /* overflow flag */
#define EFLAGS_OF                               U64_LSHIFT(1, EFLAGS_OF_BIT)
#define EFLAGS_VM_BIT                           17 /* Virtual-8086 mode */
#define EFLAGS_VM                               U64_LSHIFT(1, EFLAGS_VM_BIT)
#define EFLAGS_ID_BIT                           21 /* Identification bit */
#define EFLAGS_ID                               U64_LSHIFT(1, EFLAGS_ID_BIT)

/******************************************************************************
 * Control register related constants
******************************************************************************/

#define CR0_PE_BIT                              0 /* Enables protected mode */
#define CR0_PE                                  U64_LSHIFT(1, CR0_PE_BIT)

#define CR4_VMXE_BIT                            13 /* Enable VMX */
#define CR4_VMXE                                U64_LSHIFT(1, CR4_VMXE_BIT)

/******************************************************************************
 * General MSR related constants
******************************************************************************/

enum general_msr {
    MSR_IA32_EFER                   = 0xC0000080,
    MSRX64_IA32_FEATURE_CONTROL_MSR = 0x3A,
};

#define IA32_EFER_LME_BIT                   8 /* IA-32e mode enabled */
#define IA32_EFER_LME                       U64_LSHIFT(1, IA32_EFER_LME_BIT)
#define IA32_EFER_LMA_BIT                   10 /* IA-32e mode active */
#define IA32_EFER_LMA                       U64_LSHIFT(1, IA32_EFER_LMA_BIT)

#define MSRX64_IA32_APIC_BASE               0x1B
#define MSR_APIC_BASE_EXTD_BIT              10
#define MSR_APIC_BASE_EXTD                  (U64_LSHIFT(1, MSR_APIC_BASE_EXTD_BIT))
#define MSR_APIC_BASE_EN_BIT                11
#define MSR_APIC_BASE_EN                    (U64_LSHIFT(1, MSR_APIC_BASE_EN_BIT))

#define MSR_X2APIC_LAPIC_ID_REGISTER        0x802

#define MSRX64_FTR_CTRL_LOCK_BIT            0
#define MSRX64_FTR_CTRL_LOCKED              (U64_LSHIFT(1, MSRX64_FTR_CTRL_LOCK_BIT))
#define MSRX64_FTR_CTRL_VMX_IN_SMX_BIT      1
#define MSRX64_FTR_CTRL_VMX_IN_SMX          (U64_LSHIFT(1, MSRX64_FTR_CTRL_VMX_IN_SMX_BIT))
#define MSRX64_FTR_CTRL_VMX_OUTSIDE_SMX_BIT 2
#define MSRX64_FTR_CTRL_VMX_OUTSIDE_SMX     (U64_LSHIFT(1, MSRX64_FTR_CTRL_VMX_OUTSIDE_SMX_BIT))

/******************************************************************************
 * VMX MSR related constants
******************************************************************************/

enum vmx_msr {
    MSRX64_IA32_VMX_BASIC               = 0x480,
    MSRX64_IA32_VMX_MISC                = 0x485,

    MSRX64_IA32_VMX_CR0_FIXED0          = 0x486,
    MSRX64_IA32_VMX_CR0_FIXED1          = 0x487,
    MSRX64_IA32_VMX_CR4_FIXED0          = 0x488,
    MSRX64_IA32_VMX_CR4_FIXED1          = 0x489,

    MSRX64_IA32_VMX_VMCS_ENUM           = 0x48A,
    /**
     * This MSR exists only if
     *  - bit 63 of VMX_PROCBASED_CTLS is 1 (secondary procbased ctls)
     *  - bit 33 or 37 of VMX_PROCBASED_CTLS2 is 1
     */
    MSRX64_IA32_VMX_EPT_VPID_CAP        = 0x48C,
    MSRX64_IA32_VMX_VMFUNC              = 0x491,

    MSR_IA32_VMX_PINBASED_CTLS          = 0x481,
    /* MSR exists only if bit 55 in IA32_VMX_BASIC is 1 */
    MSR_IA32_VMX_TRUE_PINBASED_CTLS     = 0x48D,
    MSR_IA32_VMX_PROCBASED_CTLS         = 0x482,
    /* MSR exists only if bit 55 in IA32_VMX_BASIC is 1 */
    MSR_IA32_VMX_TRUE_PROCBASED_CTLS    = 0x48E,
    /* MSR exists only if bit 63 in IA32_VMX_PROCBASED_CTLS is 1 */
    MSR_IA32_VMX_PROCBASED_CTLS2        = 0x48B,
    /* MSR exists only if bit 49 in IA32_VMX_PROCBASED_CTLS is 1 */
    MSR_IA32_VMX_PROCBASED_CTLS3        = 0x492,

    MSR_IA32_VMX_EXIT_CTLS              = 0x483,
    /* MSR exists only if bit 55 in IA32_VMX_BASIC is 1 */
    MSR_IA32_VMX_TRUE_EXIT_CTLS         = 0x48F,
    MSR_IA32_VMX_EXIT_CTLS2             = 0x493,

    MSR_IA32_VMX_ENTRY_CTLS             = 0x484,
    /* MSR exists only if bit 55 in IA32_VMX_BASIC is 1 */
    MSR_IA32_VMX_TRUE_ENTRY_CTLS        = 0x490,
};

enum vmx_basic_msr_bit_pos {
    VMX_BASIC_ADDR_WIDTH_BIT        = 48,
    VMX_BASIC_DEFAULT1_MAY_BE_0_BIT = 55,
};

#define VMX_BASIC_ADDR_WIDTH        U64_LSHIFT(1, VMX_BASIC_ADDR_WIDTH_BIT)
/* Indicates whether any default1 values may be 0 (Appendix A.2) */
#define VMX_BASIC_DEFAULT1_CTLS_MAY_BE_0 \
    U64_LSHIFT(1, VMX_BASIC_DEFAULT1_MAY_BE_0_BIT)

enum vmx_ept_vpid_cap_msr_bit_pos {
    EPT_VPID_CAP_EXECUTE_ONLY_TRANSLATIONS_BIT  = 0,
    EPT_VPID_CAP_PAGE_WALK_LEN4_BIT             = 6,
    EPT_VPID_CAP_PAGE_WALK_LEN5_BIT             = 7,
    EPT_VPID_CAP_MEM_TYPE_UC_BIT                = 8,
    EPT_VPID_CAP_MEM_TYPE_WB_BIT                = 14,
    EPT_VPID_CAP_MB_PAGES_BIT                   = 16,
    EPT_VPID_CAP_GB_PAGES_BIT                   = 17,
    EPT_VPID_CAP_ACCESSED_DIRTY_BIT             = 21,
    EPT_VPID_CAP_ADVANCED_EPT_VIOLATIONS_BIT    = 22,
};

//#define VMX_BASIC_REV_ID_LEN        31
//#define VMX_BASIC_REV_ID_MASK       U64_X_LSBS_SET(VMX_BASIC_REV_ID_LEN)

#endif /* _HYVEMIND_X64_ASM_CPUFEATURES_H */

