#ifndef _HYVEMIND_X64_CPUFEATURES_H
#define _HYVEMIND_X64_CPUFEATURES_H

#include "types.h"

extern const uint32_t cpuid_range_base;
extern uint32_t cpuid_max_leaf;
extern const uint32_t cpuid_extended_range_base;
extern uint32_t cpuid_max_extended_leaf;

#define CPUID_VMX_BIT                           5 /* cpuid_result.ecx */
#define CPUID_VMX                               (U32(1) << CPUID_VMX_BIT)
#define CPUID_MSR_BIT                           5 /* cpuid_result.edx */
#define CPUID_MSR                               (U32(1) << CPUID_MSR_BIT)

#define EFLAGS_VM_BIT                           17 /* Virtual-8086 mode */
#define EFLAGS_VM                               U64_LSHIFT(1, EFLAGS_VM_BIT)
#define EFLAGS_ID_BIT                           21 /* Identification bit */
#define EFLAGS_ID                               U64_LSHIFT(1, EFLAGS_ID_BIT)

/* Relevant control registers */
#define CR0_PE_BIT                              0 /* Enables protected mode */
#define CR0_PE                                  U64_LSHIFT(1, CR0_PE_BIT)

#define CR4_VMXE_BIT                            13 /* Enable VMX */
#define CR4_VMXE                                U64_LSHIFT(1, CR4_VMXE_BIT)

/* Relevant model specific registers and fields */
#define MSRX64_IA32_EFER                    0xC0000080

#define IA32_EFER_LME_BIT                   8 /* IA-32e mode enabled */
#define IA32_EFER_LME                       U64_LSHIFT(1, IA32_EFER_LME_BIT)
#define IA32_EFER_LMA_BIT                   10 /* IA-32e mode active */
#define IA32_EFER_LMA                       U64_LSHIFT(1, IA32_EFER_LMA_BIT)

#define MSRX64_IA32_FEATURE_CONTROL_MSR     0x3A
#define MSRX64_IA32_VMX_BASIC               0x480
#define MSRX64_IA32_VMX_MISC                0x485
#define MSRX64_IA32_VMX_CR0_FIXED0          0x486
#define MSRX64_IA32_VMX_CR0_FIXED1          0x487
#define MSRX64_IA32_VMX_CR4_FIXED0          0x488
#define MSRX64_IA32_VMX_CR4_FIXED1          0x489
#define MSRX64_IA32_VMX_VMCS_ENUM           0x48A
#define MSRX64_IA32_VMX_EPT_VPID_CAP        0x48C
#define MSRX64_IA32_VMX_VMFUNC              0x491

#define VMX_BASIC_REV_ID_LEN        31
#define VMX_BASIC_REV_ID_MASK       U64_X_LSBS_SET(VMX_BASIC_REV_ID_LEN)

#define VMX_BASIC_ADDR_WIDTH_BIT    48
#define VMX_BASIC_ADDR_WIDTH        U64_LSHIFT(1, VMX_BASIC_ADDR_WIDTH_BIT)

#endif /* _HYVEMIND_X64_CPUFEATURES_H */

