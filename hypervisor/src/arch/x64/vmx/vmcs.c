#include "printf.h"
#include "pf_alloc.h"
#include "asm/paging.h"
#include "vmx/policy.h"
#include "vmx/vmcs.h"
#include "vmx/vmx_ops.h"

extern void tag_region_with_vmx_revisionid(phys_addr_t region);

phys_addr_t
create_new_vmcs_area(void)
{
    virt_addr_t vaddr;

    if (get_page_zeroed(&vaddr) != 0) {
        pr_warn("Failed to allocate page for a new VMCS area");
        return 0;
    }

    const phys_addr_t vmcs_area_ptr = virt_to_phys(vaddr);
    tag_region_with_vmx_revisionid(vmcs_area_ptr);

    return vmcs_area_ptr;
}

#define VMCS_DUMP_SECTION_HDR(title) pr_debug("\n***** " #title " *****\n");

#undef DUMP
#define DUMP(enc)                                                       \
    do {                                                                \
        uint64_t val;                                                   \
        int ret = vmread((enc), &val);                                  \
        if (ret != VMX_OP_SUCCESS) {                                    \
            __printf("VMREAD faild for %s: %lu\n", #enc, U64(ret));     \
        } else {                                                        \
            __printf("%s = 0x%lx\n", #enc, U64(val));                   \
        }                                                               \
    } while (0)

#define DUMP_GUEST_SEGMENT(seg)                                         \
    do {                                                                \
        DUMP(GUEST_ ## seg ## _SELECTOR);                               \
        DUMP(GUEST_ ## seg ## _BASE);                                   \
        DUMP(GUEST_ ## seg ## _LIMIT);                                  \
        DUMP(GUEST_ ## seg ## _ACCESS_RIGHTS);                          \
    } while (0)

void
dump_vmcs(void)
{
    uint64_t ret = 0;
    uint64_t exit_reason_raw = 0;
    vmcs_exit_reason_t exit_reason;
    uint64_t exit_qualification_raw = 0;
    union vmcs_exit_qualification exit_qualification;

    pr_debug("\n ******** VMCS DUMP START ********\n");

    VMCS_DUMP_SECTION_HDR("VM-exit information");

    if ((ret = vmread(EXIT_REASON, &exit_reason_raw)) != VMX_OP_SUCCESS) {
        __printf("VMREAD failed for EXIT_REASON: %lu\n", ret);
    } else {
        exit_reason.raw = exit_reason_raw;
        __printf("EXIT_REASON_RAW = 0x%lx\n", exit_reason_raw);
        __printf("BASIC_EXIT_REASON = 0x%lx\n", U64(exit_reason.basic_exit_reason));
        __printf("VM_ENTRY_FAILURE = 0x%lx\n", U64(exit_reason.vm_entry_failure));
    }

    /* TODO: add specific functions for displaying e.g. ept violation qualification, etc. */
    if ((ret = vmread(EXIT_QUALIFICATION, &exit_qualification_raw)) != VMX_OP_SUCCESS) {
        __printf("VMREAD failed for EXIT_QUALIFICATION: %lu\n", ret);
    } else {
        __printf("EXIT_QUALIFICATION_RAW = 0x%lx\n", exit_qualification_raw);
    }

    DUMP(EXITING_EVENT_IDENTIFICATION);
    DUMP(EXITING_EVENT_ERROR_CODE);
    DUMP(ORIGINAL_EVENT_IDENTIFICATION);
    DUMP(ORIGINAL_EVENT_ERROR_CODE);
    DUMP(VM_EXIT_INSTRUCTION_LENGTH);
    DUMP(VM_EXIT_INSTRUCTION_INFORMATION);

    DUMP(GUEST_PHYSICAL_ADDRESS);
    DUMP(GUEST_LINEAR_ADDRESS);

    VMCS_DUMP_SECTION_HDR("Pin ctls information");

    DUMP(PIN_BASED_VM_EXECUTION_CONTROLS);

    VMCS_DUMP_SECTION_HDR("Proc-based ctls information");

    DUMP(PRIMARY_PROC_BASED_VM_EXEC_CONTROLS);

    if (vmx_caps_is_option_supported(VMX_POLICY_PROCBASED_CTLS2)) {
        DUMP(SECONDARY_PROC_BASED_VM_EXEC_CONTROLS);
    } else {
        __printf("Proc based ctls2 are not supported!\n");
    }

    if (vmx_caps_is_option_supported(VMX_POLICY_PROCBASED_CTLS3)) {
        DUMP(TERT_PROC_BASED_VM_EXEC_CONTROLS);
    } else {
        __printf("Proc based ctls3 are not supported!\n");
    }

    VMCS_DUMP_SECTION_HDR("Misc VM execution controls information");

    DUMP(EXCEPTION_BITMAP);
    DUMP(PAGE_FAULT_ERROR_CODE_MASK);
    DUMP(PAGE_FAULT_ERROR_CODE_MATCH);

    DUMP(VIRTUAL_PROCESSOR_ID);
    DUMP(EPT_POINTER);
    DUMP(EPTP_INDEX);

    DUMP(ADDRESS_IO_BITMAP_A);
    DUMP(ADDRESS_IO_BITMAP_B);

    DUMP(ADDRESS_MSR_BITMAPS);

    DUMP(VIRTUAL_APIC_PAGE);
    DUMP(APIC_ACCESS_PAGE);
    DUMP(TPR_TRESHOLD);

    DUMP(VM_FUNCTION_CONTROLS);
    DUMP(INSTRUCTION_TIMEOUT_CONTROL);

    VMCS_DUMP_SECTION_HDR("VM-exit ctls information");

    DUMP(PRIMARY_VM_EXIT_CONTROLS);

    if (vmx_caps_is_option_supported(VMX_POLICY_VM_EXIT_CTLS2)) {
        DUMP(SECONDARY_VM_EXIT_CONTROLS);
    } else {
        __printf("Vm exit ctls2 are not supported!\n");
    }

    DUMP(VM_EXIT_MSR_STORE_COUNT);
    DUMP(VM_EXIT_MSR_STORE_ADDRESS);
    DUMP(VM_EXIT_MSR_LOAD_COUNT);
    DUMP(VM_EXIT_MSR_LOAD_ADDRESS);

    VMCS_DUMP_SECTION_HDR("VM-entry ctls information");

    DUMP(VM_ENTRY_CONTROLS);

    DUMP(VM_ENTRY_MSR_LOAD_COUNT);
    DUMP(VM_ENTRY_MSR_LOAD_ADDRESS);
    DUMP(INJECTED_EVENT_IDENTIFICATION);
    DUMP(INJECTED_EVENT_ERROR_CODE);
    DUMP(VM_ENTRY_INSTRUCTION_LENGTH);

    VMCS_DUMP_SECTION_HDR("Guest control registers info");

    DUMP(GUEST_CR0);
    DUMP(GUEST_CR3);
    DUMP(GUEST_CR4);
    DUMP(GUEST_DR7);

    VMCS_DUMP_SECTION_HDR("Guest execution state info");

    DUMP(GUEST_RIP);
    DUMP(GUEST_RSP);
    DUMP(GUEST_RFLAGS);

    DUMP(GUEST_ACTIVITY_STATE);
    DUMP(GUEST_INTERRUPTIBILITY_STATE);
    DUMP(GUEST_PENDING_DEBUG_EXCEPTIONS);
    DUMP(VMX_PREEMPTION_TIMER_VALUE);
    DUMP(GUEST_INTERRUPT_STATUS);
    DUMP(VMCS_LINK_POINTER);

    VMCS_DUMP_SECTION_HDR("Guest segment info");

    DUMP_GUEST_SEGMENT(CS);
    DUMP_GUEST_SEGMENT(SS);
    DUMP_GUEST_SEGMENT(ES);
    DUMP_GUEST_SEGMENT(DS);
    DUMP_GUEST_SEGMENT(FS);
    DUMP_GUEST_SEGMENT(GS);
    DUMP_GUEST_SEGMENT(LDTR);
    DUMP_GUEST_SEGMENT(TR);

    VMCS_DUMP_SECTION_HDR("Guest descriptor tables info");

    DUMP(GUEST_GDTR_BASE);
    DUMP(GUEST_GDTR_LIMIT);
    DUMP(GUEST_IDTR_BASE);
    DUMP(GUEST_IDTR_LIMIT);

    VMCS_DUMP_SECTION_HDR("Guest MSR state info");

    DUMP(GUEST_IA32_DEBUGCTL);
    DUMP(GUEST_IA32_EFER);
    DUMP(GUEST_IA32_PAT);

    DUMP(GUEST_IA32_SYSENTER_CS);
    DUMP(GUEST_IA32_SYSENTER_ESP);
    DUMP(GUEST_IA32_SYSENTER_EIP);

    VMCS_DUMP_SECTION_HDR("Host control registers info");

    DUMP(HOST_CR0);
    DUMP(HOST_CR3);
    DUMP(HOST_CR4);

    VMCS_DUMP_SECTION_HDR("Host segment info");

    DUMP(HOST_CS_SELECTOR);
    DUMP(HOST_SS_SELECTOR);
    DUMP(HOST_ES_SELECTOR);
    DUMP(HOST_DS_SELECTOR);
    DUMP(HOST_FS_SELECTOR);
    DUMP(HOST_GS_SELECTOR);
    DUMP(HOST_TR_SELECTOR);

    DUMP(HOST_FS_BASE);
    DUMP(HOST_GS_BASE);
    DUMP(HOST_TR_BASE);

    VMCS_DUMP_SECTION_HDR("Host descriptor tables info");

    DUMP(HOST_GDTR_BASE);
    DUMP(HOST_IDTR_BASE);

    VMCS_DUMP_SECTION_HDR("Host execution state info");

    DUMP(HOST_RSP);
    DUMP(HOST_RIP);

    VMCS_DUMP_SECTION_HDR("Host MSR info");

    DUMP(HOST_IA32_PAT);
    DUMP(HOST_IA32_EFER);

    DUMP(HOST_IA32_SYSENTER_CS);
    DUMP(HOST_IA32_SYSENTER_ESP);
    DUMP(HOST_IA32_SYSENTER_EIP);

    pr_debug("\n ******** VMCS DUMP END ********\n");
}

#undef DUMP
#undef DUMP_GUEST_SEGMENT
#undef VMCS_DUMP_SECTION_HDR

