#ifndef _HYVEMIND_X64_VMX_VMCS_H
#define _HYVEMIND_X64_VMX_VMCS_H

#include "hyvstdlib.h"

#define NO_CURRENT_VMCS_ADDR 0xFFFFFFFFFFFFFFFF

enum vmcs_launch_state {
    VMCS_LS_INVALID,
    VMCS_LS_CLEAR,
    VMCS_LS_LAUNCHED,
};

struct vmcs_hdr {
    uint32_t revision_id: 31,
             shadow_vmcs: 1;
};

struct vmcs {
    struct vmcs_hdr header;
    uint32_t abort_indicator;
    uint8_t data[];
};

struct vmcs_component_encoding {
    uint32_t access_type    : 1,
             index          : 9,
             type           : 2,
             reserved0      : 1,
             width          : 2,
             reserved1      : 17;
};

#define VM_INS_ERROR_MIN_VALUE 1
#define VM_INS_ERROR_MAX_VALUE 28

enum vm_instruction_error {
    VMCS_INS_NO_ERROR,
    VMCALL_IN_VMX_ROOT,
    VMCLEAR_WITH_INVALID_PADDR,
    VMCLEAR_WITH_VMXON_PTR,
    VMLAUNCH_WITH_NONCLEAR_VMCS,
    VMRESUME_WITH_NONLAUNCHED_VMCS,
    VMRESUME_AFTER_VMXOFF,
    VM_ENTRY_WITH_INVALID_CTRL_FIELDS,
    VM_ENTRY_WITH_INVALID_HOST_STATE_FIELDS,
    VMPTRLD_WITH_INVALID_PADDR,
    VMPTRLD_WITH_VMXON_PTR,
    VMPTRLD_WITH_INVALID_VMCS_REVISION_ID,
    VMREAD_VMWRITE_TO_UNSUPPORTED_VMCS_COMPONENT,
    VMWMRITE_TO_RDONLY_VMCS_COMPONENT,
    VMXON_IN_VMX_ROOT,
    VM_ENTRY_WITH_INVALID_EXECUTIVE_VMCS_PTR,
    VM_ENTRY_WITH_NONLAUNCHED_EXECUTIVE_VMCS,
    VM_ENTRY_WITH_EXECUTIVE_VMCS_PTR_NOT_VMXON_PTR,
    VMCALL_WITH_NONCLEAR_VMCS,
    VMCALL_WITH_INVALID_VM_EXIT_CTRL_FIELDS,
    VMCALL_WITH_INCORRECT_MSEG_REV_ID,
    VMXOFF_UNDER_DUAL_MONITOR,
    VMCALL_WITH_INVALID_SMM_FTRS,
    VM_ENTRY_WITH_INVALID_EXECUTION_CTRL_FIELDS_IN_EXECUTIVE_VMCS,
    VM_ENTRY_WITH_EVENTS_BLOCKED_BY_MOV_SS,
    INVALID_OPERAND_TO_INVEPT_INVVPID
};

static inline bool
is_valid_vm_ins_error(const int val)
{
    return (VM_INS_ERROR_MIN_VALUE <= val) && (val <= VM_INS_ERROR_MAX_VALUE);
}

enum vmcs_field_encoding: uint64_t {
    /* 16 bit fields */
    VIRTUAL_PROCESSOR_ID                    = 0x00000000,
    POSTED_INTERRUPT_NOTIFICATION_VECTOR    = 0x00000002,
    EPTP_INDEX                              = 0x00000004,

    GUEST_ES_SELECTOR                       = 0x00000800,
    GUEST_CS_SELECTOR                       = 0x00000802,
    GUEST_SS_SELECTOR                       = 0x00000804,
    GUEST_DS_SELECTOR                       = 0x00000806,
    GUEST_FS_SELECTOR                       = 0x00000808,
    GUEST_GS_SELECTOR                       = 0x0000080a,
    GUEST_LDTR_SELECTOR                     = 0x0000080c,
    GUEST_TR_SELECTOR                       = 0x0000080e,
    GUEST_INTERRUPT_STATUS                  = 0x00000810,

    HOST_ES_SELECTOR                        = 0x00000c00,
    HOST_CS_SELECTOR                        = 0x00000c02,
    HOST_SS_SELECTOR                        = 0x00000c04,
    HOST_DS_SELECTOR                        = 0x00000c06,
    HOST_FS_SELECTOR                        = 0x00000c08,
    HOST_GS_SELECTOR                        = 0x00000c0a,
    HOST_TR_SELECTOR                        = 0x00000c0c,

    /* 64 bit fields (encodings for access type FULL) */
    ADDRESS_IO_BITMAP_A                     = 0x00002000,
    ADDRESS_IO_BITMAP_B                     = 0x00002002,
    ADDRESS_MSRR_BITMAPS                    = 0x00002004,
    VM_EXIT_MSR_STORE_ADDRESS               = 0x00002006,
    VM_EXIT_MSR_LOAD_ADDRESS                = 0x00002008,
    VM_ENTRY_MSR_LOAD_ADDRESS               = 0x0000200a,
    VIRTUAL_APIC_ADDRESS                    = 0x00002012,
    APIC_ACCESS_ADDRESS                     = 0x00002014,
    VM_FUNCTION_CONTROLS                    = 0x00002018,
    EPT_POINTER                             = 0x0000201a,
    VMREAD_BITMAP_ADDRESS                   = 0x00002026,
    VMWRITE_BITMAP_ADDRESS                  = 0x00002028,
    TERT_PROC_BASED_VM_EXEC_CONTROLS        = 0x00002034,
    SECONDARY_VM_EXIT_CONTROLS              = 0x00002044,
    GUEST_PHYSICAL_ADDRESS                  = 0x00002400,
    VMCS_LINK_POINTER                       = 0x00002800,
    GUEST_IA32_PAT                          = 0x00002804,
    GUEST_IA32_EFER                         = 0x00002806,
    GUEST_PDPTE0                            = 0x0000280a,
    GUEST_PDPTE1                            = 0x0000280c,
    GUEST_PDPTE2                            = 0x0000280e,
    GUEST_PDPTE3                            = 0x00002810,
    HOST_IA32_PAT                           = 0x00002c00,
    HOST_IA32_EFER                          = 0x00002c02,

    /* 32 bit fields */
    PIN_BASED_VM_EXECUTION_CONTROLS         = 0x00004000,
    PRIMARY_PROC_BASED_VM_EXEC_CONTROLS     = 0x00004002,
    EXCEPTION_BITMAP                        = 0x00004004,
    PAGE_FAULT_ERROR_CODE_MASK              = 0x00004006,
    PAGE_FAULT_ERROR_CODE_MATCH             = 0x00004006,
    PRIMARY_VM_EXIT_CONTROLS                = 0x0000400c,
    VM_EXIT_MSR_STORE_COUNT                 = 0x0000400e,
    VM_EXIT_MSR_LOAD_COUNT                  = 0x00004010,
    VM_ENTRY_CONTROLS                       = 0x00004012,
    VM_ENTRY_MSR_LOAD_COUNT                 = 0x00004014,
    INJECTED_EVENT_IDENTIFICATION           = 0x00004016,
    INJECTED_EVENT_ERROR_CODE               = 0x00004018,
    VM_ENTRY_INSTRUCTION_LENGTH             = 0x0000401a,
    SECONDARY_PROC_BASED_VM_EXEC_CONTROLS   = 0x0000401e,
    INSTRUCTION_TIMEOUT_CONTROL             = 0x00004024,
    VM_INSTRUCTION_ERROR                    = 0x00004400,
    EXIT_REASON                             = 0x00004402,
    EXITING_EVENT_IDENTIFICATION            = 0x00004404,
    EXITING_EVENT_ERROR_CODE                = 0x00004406,
    ORIGINAL_EVENT_IDENTIFICATION           = 0x00004408,
    ORIGINAL_EVENT_ERROR_CODE               = 0x0000440a,
    VM_EXIT_INSTRUCTION_LENGTH              = 0x0000440c,
    VM_EXIT_INSTRUCTION_INFORMATION         = 0x0000440e,
    GUEST_ES_LIMIT                          = 0x00004800,
    GUEST_CS_LIMIT                          = 0x00004802,
    GUEST_SS_LIMIT                          = 0x00004804,
    GUEST_DS_LIMIT                          = 0x00004806,
    GUEST_FS_LIMIT                          = 0x00004808,
    GUEST_GS_LIMIT                          = 0x0000480a,
    GUEST_LDTR_LIMIT                        = 0x0000480c,
    GUEST_TR_LIMIT                          = 0x0000480e,
    GUEST_GDTR_LIMIT                        = 0x00004810,
    GUEST_IDTR_LIMIT                        = 0x00004812,
    GUEST_ES_ACCESS_RIGHTS                  = 0x00004814,
    GUEST_CS_ACCESS_RIGHTS                  = 0x00004816,
    GUEST_SS_ACCESS_RIGHTS                  = 0x00004818,
    GUEST_DS_ACCESS_RIGHTS                  = 0x0000481a,
    GUEST_FS_ACCESS_RIGHTS                  = 0x0000481c,
    GUEST_GS_ACCESS_RIGHTS                  = 0x0000481e,
    GUEST_LDTR_ACCESS_RIGHTS                = 0x00004820,
    GUEST_TR_ACCESS_RIGHTS                  = 0x00004822,
    GUEST_INTERRUPTIBILITY_STATE            = 0x00004824,
    GUEST_ACTIVITY_STATE                    = 0x00004826,
    GUEST_IA32_SYSENTER_CS                  = 0x0000482a,
    VMX_PREEMPTION_TIMER_VALUE              = 0x0000482e,
    HOST_IA32_SYSENTER_CS                   = 0x00004c00,

    /* Normal width fields */
    EXIT_QUALIFICATION                      = 0x00006400,
    IO_RCX                                  = 0x00006402,
    IO_RSI                                  = 0x00006404,
    IO_RDI                                  = 0x00006406,
    IO_RIP                                  = 0x00006408,
    GUEST_LINEAR_ADDRESS                    = 0x0000640a,
    GUEST_CR0                               = 0x00006800,
    GUEST_CR3                               = 0x00006802,
    GUEST_CR4                               = 0x00006804,
    GUEST_ES_BASE                           = 0x00006806,
    GUEST_CS_BASE                           = 0x00006808,
    GUEST_SS_BASE                           = 0x0000680a,
    GUEST_DS_BASE                           = 0x0000680c,
    GUEST_FS_BASE                           = 0x0000680e,
    GUEST_GS_BASE                           = 0x00006810,
    GUEST_LDTR_BASE                         = 0x00006812,
    GUEST_TR_BASE                           = 0x00006814,
    GUEST_GDTR_BASE                         = 0x00006816,
    GUEST_IDTR_BASE                         = 0x00006818,
    GUEST_DR7                               = 0x0000681a,
    GUEST_RSP                               = 0x0000681c,
    GUEST_RIP                               = 0x0000681e,
    GUEST_RFLAGS                            = 0x00006820,
    GUEST_PENDING_DEBUG_EXCEPTIONS          = 0x00006822,
    GUEST_IA32_SYSENTER_ESP                 = 0x00006824,
    GUEST_IA32_SYSENTER_EIP                 = 0x00006826,
    HOST_CR0                                = 0x00006c00,
    HOST_CR3                                = 0x00006c02,
    HOST_CR4                                = 0x00006c04,
    HOST_FS_BASE                            = 0x00006c06,
    HOST_GS_BASE                            = 0x00006c08,
    HOST_TR_BASE                            = 0x00006c0a,
    HOST_GDTR_BASE                          = 0x00006c0c,
    HOST_IDTR_BASE                          = 0x00006c0e,
    HOST_IA32_SYSENTER_ESP                  = 0x00006c10,
    HOST_IA32_SYSENTER_EIP                  = 0x00006c12,
    HOST_RSP                                = 0x00006c14,
    HOST_RIP                                = 0x00006c16,
};
typedef uint32_t vmcs_field_encoding_t;

void vmcs_write_field(const vmcs_field_encoding_t encoding, const uint64_t value);
#define vmcs_write_16bit_field(encoding, value) vmcs_write_field(encoding, value)
#define vmcs_write_32bit_field(encoding, value) vmcs_write_field(encoding, value)
#define vmcs_write_64bit_field(encoding, value) vmcs_write_field(encoding, value)
#define vmcs_write_natural_field(encoding, value) vmcs_write_field(encoding, value)

uint64_t vmcs_read_field(const vmcs_field_encoding_t encoding);
#define vmcs_read_16bit_field(encoding) vmcs_read_field(encoding)
#define vmcs_read_32bit_field(encoding) vmcs_read_field(encoding)
#define vmcs_read_64bit_field(encoding) vmcs_read_field(encoding)
#define vmcs_read_natural_field(encoding) vmcs_read_field(encoding)

#endif /* _HYVEMIND_X64_VMX_VMCS_H */

