#ifndef _HYVEMIND_X64_VMX_VMX_DEFS_H
#define _HYVEMIND_X64_VMX_VMX_DEFS_H

#define VMX_OP_INTEL_MIN_ERR 1
#define VMX_OP_INTEL_MAX_ERR 28

#define VMX_SUCCESS 0

#define VMX_OP_VMCALL_IN_VMX_ROOT                           VMX_OP_INTEL_MIN_ERR
#define VMX_OP_VMCLEAR_INVALID_PADDR                        2
#define VMX_OP_VMCLEAR_VMXON_PTR                            3
#define VMX_OP_VMLAUNCH_NONCLEAR_VMCS                       4
#define VMX_OP_VMRESUME_NONLAUNCHED_VMCS                    5
#define VMX_OP_VMRESUME_AFTER_VMXOFF                        6
#define VMX_OP_VM_ENTRY_INVALID_CTRLS                       7
#define VMX_OP_VM_ENTRY_INVALID_HOST_STATE                  8
#define VMX_OP_VMPTRLD_INVALID_PADDR                        9
#define VMX_OP_VMPTRLD_VMXON_PTR                            10
#define VMX_OP_VMPTRLD_INVALID_VMCS_REVISION_ID             11
#define VMX_OP_UNSUPPORTED_VMCS_FIELD_ACCESS                12
#define VMX_OP_VMWMRITE_TO_RDONLY_FIELD                     13
#define VMX_OP_VMXON_IN_VMX_ROOT                            15
#define VMX_OP_VM_ENTRY_INVALID_EXECUTIVE_VMCS_PTR          16
#define VMX_OP_VM_ENTRY_NONLAUNCHED_EXECUTIVE_VMCS          17
#define VMX_OP_VM_ENTRY_EXECUTIVE_VMCS_PTR_NOT_VMXON_PTR    18
#define VMX_OP_VMCALL_WITH_NONCLEAR_VMCS                    19
#define VMX_OP_VMCALL_WITH_INVALID_VM_EXIT_CTRLS            20
#define VMX_OP_VMCALL_WITH_INCORRECT_MSEG_REV_ID            22
#define VMX_OP_VMXOFF_DUAL_MONITOR                          23
#define VMX_OP_VMCALL_WITH_INVALID_SMM_FTRS                 24
#define VMX_OP_VM_ENTRY_INVALID_EXEC_CTRLS                  25
#define VMX_OP_VM_ENTRY_MOV_SS_BLOCKING                     26
#define VMX_OP_INVALID_INVEPT_INVVPID_OPERAND               VMX_OP_INTEL_MAX_ERR
#define VMX_OP_FAIL_INVALID                                 29
#define VMX_OP_UNKNOWN_ERROR                                30

/* Custom errors */
#define VMX_INVALID_LAUNCH_STATE    31
#define VMX_VMLAUNCH_FAILED         32
#define VMX_VMRESUME_FAILED         33
#define VMX_VM_ENTRY_ERROR          34
#define VMX_ERROR                   35

#endif /* _HYVEMIND_X64_VMX_VMX_DEFS_H */

