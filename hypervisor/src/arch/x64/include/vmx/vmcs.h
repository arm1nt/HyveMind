#ifndef _HYVEMIND_X64_VMX_VMCS_H
#define _HYVEMIND_X64_VMX_VMCS_H

#include "hyvstdlib.h"

struct vmcs_hdr {
    uint32_t revision_id: 31,
             shadow_vmcs: 1;
};

struct vmcs {
    struct vmcs_hdr header;
    uint32_t abort_indicator;
    uint8_t data[];
};

#define NO_CURRENT_VMCS_ADDR    0xFFFFFFFFFFFFFFFF
#define NO_VMCS_LINK_PTR        0xFFFFFFFFFFFFFFFF

enum vmcs_launch_state {
    VMCS_LS_INVALID,
    VMCS_LS_CLEAR,
    VMCS_LS_LAUNCHED,
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

union guest_state_access_rights {
    uint32_t raw;
    struct {
        uint32_t segment_type: 4,
                 descriptor_type: 1,
                 dpl: 2,
                 present: 1,
                 reserved0: 4,
                 avl: 1,
                 l: 1,
                 db: 1,
                 g: 1,
                 segment_unusable: 1,
                 reserved1: 15;
    };
};

enum vmcs_basic_exit_reason {
    EXIT_REASON_EXCEPTION,
    EXIT_REASON_EXT_INTR,
    EXIT_REASON_TRIPLE_FAULT,
    EXIT_REASON_INIT,
    EXIT_REASON_SIPI,
    EXIT_REASON_IO_SMI,
    EXIT_REASON_OTHER_SMI,
    EXIT_REASON_INTR_WINDOW,
    EXIT_REASON_NMI_WINDOW,
    EXIT_REASON_TASK_SWITCH,
    EXIT_REASON_CPUID,
    EXIT_REASON_GETSEC,
    EXIT_REASON_HLT,
    EXIT_REASON_INVD,
    EXIT_REASON_INVLPG,
    EXIT_REASON_RDPMC,
    EXIT_REASON_RDTSC,
    EXIT_REASON_RSM,
    EXIT_REASON_VMCALL,
    EXIT_REASON_VMCLEAR,
    EXIT_REASON_VMLAUNCH,
    EXIT_REASON_VMPTRLD,
    EXIT_REASON_VMPTRST,
    EXIT_REASON_VMREAD,
    EXIT_REASON_VMRESUME,
    EXIT_REASON_VMWRITE,
    EXIT_REASON_VMXOFF,
    EXIT_REASON_VMXON,
    EXIT_REASAON_CR_ACCESS,
    EXIT_REASON_MOV_DR,
    EXIT_REASON_IO_INSTRUCTION,
    EXIT_REASON_RDMSR_IMPLICIT,
    EXIT_REASON_WRMSR_IMPLICIT_WRMSRNS,
    EXIT_REASON_VM_ENTRY_INV_GUEST_STATE,
    EXIT_REASON_VM_ENTRY_MSR_LOADING,
    EXIT_REASON_MWAIT,
    EXIT_REASON_MONITOR_TRAP_FLAG,
    EXIT_REASON_MONITOR,
    EXIT_REASON_PAUSE,
    EXIT_REASON_VM_ENTRY_MACHINE_CHECK,
    EXIT_REASON_TRP_BELOW_THRESHOLD,
    EXIT_REASON_APIC_ACCESS,
    EXIT_REASON_VIRTUALIZED_EOI,
    EXIT_REASON_GDTR_IDTR_ACCESS,
    EXIT_REASON_LDTR_TR_ACCESS,
    EXIT_REASON_EPT_VIOLATION,
    EXIT_REASON_EPT_MISCONFIG,
    EXIT_REASON_INVEPT,
    EXIT_REASON_RDTSCP,
    EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED,
    EXIT_REASON_INVVPID,
    EXIT_REASON_WBINVD_WBNOINVD,
    EXIT_REASON_XSETBV,
    EXIT_REASON_APIC_WRITE,
    EXIT_REASON_RDRAND,
    EXIT_REASON_INVPCID,
    EXIT_REASON_VMFUNC,
    EXIT_REASON_ENCLS,
    EXIT_REASON_RDSEED,
    EXIT_REASON_PAGE_MOD_LOG_FULL,
    EXIT_REASON_XSAVES,
    EXIT_REASON_XRSTORS,
    EXIT_REASON_PCONFIG,
    EXIT_REASON_SPP_RELATED_EVENT,
    EXIT_REASON_UMWAIT,
    EXIT_REASON_TPAUSE,
    EXIT_REASON_LOADIWKEY,
    EXIT_REASON_ENQCMD_PASID_TRANSLATION,
    EXIT_REASON_ENQCMDS_PASID_TRANSLATION,
    EXIT_REASON_BUS_LOCK,
    EXIT_REASON_INSTRUCTION_TIMEOUT,
    EXIT_REASON_SEAMCALL,
    EXIT_REASON_TDCALL,
    EXIT_REASON_RDMSRLIST,
    EXIT_REASON_WRMSRLIST,
    EXIT_REASON_URDMSR,
    EXIT_REASON_UWRMSR,
    EXIT_REASON_RDMSR_IMMEDIATE,
    EXIT_REASON_WRMSRNS_IMMEDIATE,
};

union vmcs_exit_reason {
    uint32_t raw;
    struct {
        uint32_t basic_exit_reason              : 16,
                 zero0                          : 1,
                 undefined0                     : 8,
                 shadow_stack_prematurely_busy  : 1,
                 bus_lock_asserted              : 1,
                 enclave_mode                   : 1,
                 pending_mtf_vm_exit            : 1,
                 exit_from_vmx_root             : 1,
                 undefined1                     : 1,
                 vm_entry_failure               : 1;
    };
};
typedef union vmcs_exit_reason vmcs_exit_reason_t;

#define PIN_BASED_CTL_ACTIVATE      1
#define PIN_BASED_CTL_DEACTIVATE    0

union vmcs_pin_based_ctls_vector {
    uint32_t raw;
    struct {
        uint32_t external_interrupt_handling    : 1,
                 reserved0                      : 2,
                 nmi_exiting                    : 1,
                 reserved1                      : 1,
                 virtual_nmis                   : 1,
                 activate_vmx_preemption_timer  : 1,
                 process_posted_interrupts      : 1,
                 reserved2                      : 24;
    };
};

union vmcs_primary_processor_based_ctls_vector {
    uint32_t raw;
    struct {
        uint32_t reserved0                      : 2,
                 interrupt_window_exiting       : 1,
                 tsc_offsetting                 : 1,
                 reserved1                      : 3,
                 hlt_exiting                    : 1,
                 reserved2                      : 1,
                 invlpg_exiting                 : 1,
                 mwait_exiting                  : 1,
                 rdpmc_exiting                  : 1,
                 rdtsc_exiting                  : 1,
                 reserved3                      : 2,
                 cr3_load_exiting               : 1,
                 cr3_store_exiting              : 1,
                 activate_tertiary_controls     : 1,
                 reserved4                      : 1,
                 cr8_load_exiting               : 1,
                 cr8_store_exiting              : 1,
                 tpr_shadow                     : 1,
                 nmi_window_exiting             : 1,
                 mov_dr_exiting                 : 1,
                 unconditional_io_exiting       : 1,
                 io_bitmaps                     : 1,
                 reserved5                      : 1,
                 monitor_trap_flag              : 1,
                 msr_bitmaps                    : 1,
                 monitor_exiting                : 1,
                 pause_exiting                  : 1,
                 activate_secondary_controls    : 1;
    };
};

union vmcs_secondary_processor_based_ctls_vector {
    uint32_t raw;
    struct {
        uint32_t virtualize_apic_addresses          : 1,
                 enable_ept                         : 1,
                 descriptor_table_exiting           : 1,
                 enable_rdtscp                      : 1,
                 virtualize_x2apic_mode             : 1,
                 enable_vpid                        : 1,
                 wbinvd_exiting                     : 1,
                 unrestricted_guest                 : 1,
                 apic_register_virtualization       : 1,
                 virtual_interrupt_delivery         : 1,
                 pause_loop_exiting                 : 1,
                 rdrand_exiting                     : 1,
                 enable_invpcid                     : 1,
                 enable_vm_functions                : 1,
                 vmcs_shadowing                     : 1,
                 enable_encls_exiting               : 1,
                 rdseed_exiting                     : 1,
                 enable_pml                         : 1,
                 ept_violation                      : 1,
                 conceal_vmx_from_pt                : 1,
                 enable_xsaves_xrstors              : 1,
                 pasid_translation                  : 1,
                 mode_base_execute_ctrl_for_ept     : 1,
                 sub_page_write_permissions_for_ept : 1,
                 intel_pt_uses_guest_paddrs         : 1,
                 use_tsc_scaling                    : 1,
                 enable_user_wait_pause             : 1,
                 enable_pconfig                     : 1,
                 reserved0                          : 2,
                 vmm_bus_lock_detection             : 1,
                 instruction_timeout                : 1;
    };
};

union vmcs_tertiary_processor_based_ctls_vector {
    uint64_t raw;
    struct {
        uint64_t loadiwkey_exiting              : 1,
                 enable_hlat                    : 1,
                 ept_paging_write_control       : 1,
                 guest_paging_verification      : 1,
                 ipi_virtualization             : 1,
                 seam_guest_paddr_width         : 1,
                 enable_msr_list_instructions   : 1,
                 virtualize_ia32_spec_ctrl      : 1,
                 apic_timer_virtualization      : 1,
                 enable_pbndkb                  : 1,
                 reserved0                      : 2,
                 pebs_uses_guest_paddrs         : 1,
                 reserved1                      : 51;
    };
};

union vmcs_primary_vm_exit_ctls_vector {
    uint32_t raw;
    struct {
        uint32_t reserved0                          : 2,
                 save_debug_ctrls                   : 1,
                 reserved1                          : 6,
                 host_addr_space_size               : 1,
                 reserved2                          : 2,
                 load_ia32_perf_global_ctrl         : 1,
                 reserved3                          : 2,
                 ack_interrupt_on_exit              : 1,
                 reserved4                          : 2,
                 save_ia32_pat                      : 1,
                 load_ia32_pat                      : 1,
                 save_ia32_efer                     : 1,
                 load_ia32_efer                     : 1,
                 save_vmx_preemption_timer_value    : 1,
                 clear_ia32_bndcfgs                 : 1,
                 conceal_vmx_from_pt                : 1,
                 clear_ia32_rtit_ctl                : 1,
                 clear_ia32_lbr_ctl                 : 1,
                 clear_uinv                         : 1,
                 load_cet_state                     : 1,
                 load_pkrs                          : 1,
                 save_ia32_pef_global_ctl           : 1,
                 activate_secondary_controls        : 1;
    };
};

union vmcs_secondary_vm_exit_ctls_vector {
    uint64_t raw;
    struct {
        uint64_t save_fred                      : 1,
                 load_fred                      : 1,
                 load_ia32_spec_ctrl            : 1,
                 prematurely_busy_shadow_stack  : 1,
                 reserved0                      : 60;
    };
};

union vmcs_vm_entry_ctls_vector {
    uint32_t raw;
    struct {
        uint32_t reserved0                          : 2,
                 load_debug_controls                : 1,
                 reserved1                          : 6,
                 ia32e_mode_guest                   : 1,
                 entry_to_smm                       : 1,
                 deactivate_dual_monitor_treatment  : 1,
                 reserved2                          : 1,
                 load_ia32_perf_global_ctrl         : 1,
                 load_ia32_pat                      : 1,
                 load_ia32_efer                     : 1,
                 load_ia32_bndcfgs                  : 1,
                 conceal_vmx_from_pt                : 1,
                 load_ia32_rtit_ctl                 : 1,
                 load_uinv                          : 1,
                 load_cet_state                     : 1,
                 load_guest_ia32_lbr_ctl            : 1,
                 load_pkrs                          : 1,
                 load_fred                          : 1,
                 load_ia32_spec_ctrl                : 1,
                 allow_seam_guest_telemetry         : 1,
                 reserved3                          : 6;
    };
};

enum vmcs_guest_activity_state {
    GUEST_ACTIVE,
    GUEST_HLT,
    GUEST_SHUTDOWN,
    GUEST_WAIT_FOR_SIPI
};

struct vmcs_component_encoding {
    uint32_t access_type    : 1,
             index          : 9,
             type           : 2,
             reserved0      : 1,
             width          : 2,
             reserved1      : 17;
};

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
typedef uint64_t vmcs_field_encoding_t;

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

