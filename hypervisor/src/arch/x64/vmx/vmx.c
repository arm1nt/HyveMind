#include "fatal.h"
#include "string.h"
#include "per-cpu.h"
#include "pf_alloc.h"
#include "printf.h"
#include "vm.h"
#include "asm/cpufeatures.h"
#include "asm/gdt_idt.h"
#include "asm/paging.h"
#include "asm/processor.h"
#include "asm/vmm.h"
#include "vmx/policy.h"
#include "vmx/vmx.h"
#include "vmx/vmcs.h"

DEFINE_PER_CPU(vcpu_t *, curr_vcpu);
#define current_vcpu (percpu_val(curr_vcpu))

#define VM_OP_STATUS_SUCCESS        0
/* values 1 to 28 represent specific vm instruction errors */
#define VM_OP_STATUS_FAIL_INVALID   29
#define VM_OP_STATUS_UNKNOWN_STATE  30

static inline int __check_vm_op_status(const uint32_t flags);

#define query_vm_op_status() ({                             \
        uint64_t eflags;                                    \
        asm volatile ("pushfq; pop %0" : "=r"(eflags));     \
        __check_vm_op_status(eflags);                       \
        })

static inline int
vmxon(const phys_addr_t vmxon_region)
{
    asm volatile("vmxon %0" :: "m"(vmxon_region) : "memory");
    return query_vm_op_status();
}

static inline int
vmxoff(void)
{
    asm volatile("vmxoff");
    return query_vm_op_status();
}

static inline int
vmclear(const phys_addr_t vmcs_area_ptr)
{
    asm volatile("vmclear %0" :: "m"(vmcs_area_ptr) : "memory");
    return query_vm_op_status();
}

static inline int
vmptrst(phys_addr_t *vmcs_addr_ptr)
{
    asm volatile("vmptrst %0" : "=m"(*vmcs_addr_ptr) :: "memory");
    return query_vm_op_status();
}

static inline int
vmptrld(const phys_addr_t vmcs_ptr)
{
    asm volatile("vmptrld %0" :: "m"(vmcs_ptr) : "memory");
    return query_vm_op_status();
}

/* In 64-bit mode, the destionation operand size is always 64 bit */
static inline int
vmread(const enum vmcs_field_encoding encoding, uint64_t *val)
{
    int res;
    asm volatile("vmread %1, %0" : "=m"(*val) : "r"(encoding) : "memory");
    res = query_vm_op_status();

    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'vmread' failed: %lu", res);
    }

    return res;
}

static inline int
vmwrite(const enum vmcs_field_encoding encoding, const uint64_t value)
{
    int res;
    asm volatile("vmwrite %0, %1" :: "m"(value), "r"(encoding) : "memory");
    res = query_vm_op_status();

    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'vmwrite' failed: %lu", res);
    }

    return res;
}

static inline int
vmlaunch(void)
{
    asm volatile("vmlaunch");
    return query_vm_op_status();
}

static inline int
vmresume(void)
{
    asm volatile("vmresume");
    return query_vm_op_status();
}

static inline bool
has_current_vmcs(void)
{
    int res;
    phys_addr_t curr_vmcs_ptr;

    res = vmptrst(&curr_vmcs_ptr);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'has_current_vmcs()' failed to get current vmcs ptr: %lu", res);
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

static inline int
__check_vm_op_status(const uint32_t flags)
{
    if (__is_vm_succeed(flags)) {
        return VM_OP_STATUS_SUCCESS;
    }

    if (__is_vm_fail_invalid(flags)) {
        return VM_OP_STATUS_FAIL_INVALID;
    }

    if (__is_vm_fail_valid(flags)) {
        uint64_t error_number;
        vmread(VM_INSTRUCTION_ERROR, &error_number);
        return error_number;
    }

    return VM_OP_STATUS_UNKNOWN_STATE;
}

static inline int
ensure_vcpu_current(vcpu_t *vcpu)
{
    if (vcpu == current_vcpu) {
        return 0;
    }

    const int res = vmptrld(vcpu->arch.hw.vmx.vmcs_ptr);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'vmptrld' in ensure_vcpu_current() failed: %lu", res);
        return -1;
    }

    vcpu->arch.active = true;
    vcpu->arch.active_processor = get_current_cpuid();

    set_percpu_val(curr_vcpu, vcpu);
    return 0;
}

/* TODO: improve + better error msg & handling */
static inline int
clear_vcpu(vcpu_t *vcpu)
{
    if (!(vcpu->arch.active && (vcpu->arch.active_processor == get_current_cpuid()))) {
        pr_warn("vcpu is not active on cpu '%lu'", U64(get_current_cpuid()));
        return -1;
    }

    vmclear(vcpu->arch.hw.vmx.vmcs_ptr);

    current_vcpu = NULL;
    vcpu->arch.active = false;
    vcpu->arch.active_processor = INVALID_PROCESSOR_ID;
    vcpu->arch.hw.vmx.launch_state = VMCS_LS_CLEAR;
    return 0;
}

int
vmx_vcpu_allocate(vcpu_t *vcpu)
{
    int res;
    phys_addr_t vmcs_ptr;

    vmcs_ptr = create_new_vmcs_area();
    if (!vmcs_ptr) {
        pr_error("Failed to allocate a VMCS area");
        return -1;
    }

    res = vmclear(vmcs_ptr);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_error("'vmclear' on new vmcs area failed: %lu", U64(res));
        free_page_raw(vmcs_ptr);
        return -1;
    }

    vcpu->arch.active = false;
    vcpu->arch.active_processor = INVALID_PROCESSOR_ID;
    vcpu->arch.run_status = VCPU_NOT_READY;
    vcpu->arch.hw.vmx.vmcs_ptr = vmcs_ptr;
    vcpu->arch.hw.vmx.launch_state = VMCS_LS_CLEAR;

    return 0;
}

void
vmx_destroy_vcpu(vcpu_t *vcpu)
{
    const phys_addr_t curr_vmcs_ptr = vcpu->arch.hw.vmx.vmcs_ptr;

    if ((vcpu->arch.hw.vmx.launch_state != VMCS_LS_INVALID) && curr_vmcs_ptr) {
        vmclear(curr_vmcs_ptr);
        free_page_raw(curr_vmcs_ptr);
        vcpu->arch.hw.vmx.vmcs_ptr = 0;
        vcpu->arch.hw.vmx.launch_state = VMCS_LS_INVALID;
    }
}

#define VMX_CR0_FIXED0 (read_msr(MSRX64_IA32_VMX_CR0_FIXED0))
#define VMX_CR0_FIXED1 (read_msr(MSRX64_IA32_VMX_CR0_FIXED1))
#define VMX_CR4_FIXED0 (read_msr(MSRX64_IA32_VMX_CR4_FIXED0))
#define VMX_CR4_FIXED1 (read_msr(MSRX64_IA32_VMX_CR4_FIXED1))

static inline void
sanitize_cr0_for_vmx_operation(cr0_t *cr0)
{
    const uint64_t old_cr0_raw = cr0->raw;
    cr0->raw = (cr0->raw | VMX_CR0_FIXED0) & VMX_CR0_FIXED1;

    if (old_cr0_raw != cr0->raw) {
        pr_info("CR0 value changed by sanitization.\nOld: %lx\nNew: %lx",
                old_cr0_raw,
                cr0->raw
        );
    }
}

static inline void
sanitize_cr3_for_vmx_operation(cr3_t *cr3)
{
    cr3->cr3_64b.ignored0 = 0;
    cr3->cr3_64b.ignored1 = 0;
    cr3->cr3_64b.reserved0 = 0;
    cr3->cr3_64b.reserved1 = 0;
}

static inline void
sanitize_cr4_for_vmx_operation(cr4_t *cr4)
{
    const uint64_t old_cr4_raw = cr4->raw;
    cr4->raw = (cr4->raw | VMX_CR4_FIXED0) & VMX_CR4_FIXED1;

    if (old_cr4_raw != cr4->raw) {
        pr_info("CR4 value changed by sanitization.\nOld: %lx\nNew: %lx",
                old_cr4_raw,
                cr4->raw
        );
    }
}

static inline void
sanitize_ctrl_regs_for_vmxon(void)
{
    cr0_t cr0;
    cr4_t cr4;

    cr0.raw = read_cr0();
    sanitize_cr0_for_vmx_operation(&cr0);
    write_cr0(cr0.raw);

    cr4.raw = read_cr4();
    sanitize_cr4_for_vmx_operation(&cr4);
    write_cr4(cr4.raw);
}

inline void
tag_region_with_vmx_revisionid(const phys_addr_t region)
{
    const uint64_t vmx_basic = read_msr(MSRX64_IA32_VMX_BASIC);
    const uint32_t revision_id = U64_LOWER32(vmx_basic);
    virt_addr_t vaddr = phys_to_virt(region);
    memcpy((void *) vaddr, &revision_id, sizeof(uint32_t));
}

static inline phys_addr_t
create_vmxon_region(void)
{
    virt_addr_t vaddr;
    if (get_page_zeroed(&vaddr) != 0) {
        pr_warn("Failed to allocate page for a new VMXON region");
        return 0;
    }

    const phys_addr_t vmxon_region = virt_to_phys(vaddr);
    tag_region_with_vmx_revisionid(vmxon_region);
    return vmxon_region;
}

bool
enter_vmx_operation(void)
{
    int res;
    phys_addr_t vmxon_region;
    logical_processor_t *current = get_current_logical_processor();

    if ((res = init_vmx_capabilities()) != 0) {
        pr_error("Failed to initialize the vmx capability info");
        return false;
    }

    vmxon_region = create_vmxon_region();
    if (!vmxon_region) {
        pr_error("Unable to enter VMX operation: Creating a VMXON region failed");
        return false;
    }

    sanitize_ctrl_regs_for_vmxon();

    res = vmxon(vmxon_region);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_error("Executing 'vmxon' failed with error status: %lu", U64(res));
        free_page_raw(vmxon_region);
        return false;
    }

    pr_info("Successfully entered VMX operation");
    current->vmxon_region_ptr = vmxon_region;
    current->vmx_operation_active = true;

    return true;
}

void
leave_vmx_operation(void)
{
    logical_processor_t *current = get_current_logical_processor();
    if (!current->vmx_operation_active) {
        pr_warn("Cannot execute 'vmxoff' since logical processor is not in VMX operation");
        return;
    }

    vmxoff();

    free_page_raw(current->vmxon_region_ptr);
    current->vmxon_region_ptr = 0;
    current->vmx_operation_active = false;

    pr_info("Successfully left VMX operation");
}

static inline void
setup_host_ctrl_registers(void)
{
    cr0_t cr0;
    cr3_t cr3;
    cr4_t cr4;

    cr0.raw = read_cr0();
    cr0.pg = 1;
    cr0.pe = 1;
    cr0.wp = 1;
    sanitize_cr0_for_vmx_operation(&cr0);
    vmwrite(HOST_CR0, cr0.raw);

    cr3.cr3_64b.raw = read_cr3();
    sanitize_cr3_for_vmx_operation(&cr3);
    vmwrite(HOST_CR3, cr3.cr3_64b.raw);

    cr4.raw = read_cr4();
    cr4.pae = 1;
    sanitize_cr4_for_vmx_operation(&cr4);
    vmwrite(HOST_CR4, cr4.raw);
}

static inline void
setup_host_segment_registers(void)
{
    const segment_selector_t cs = read_cs_register();
    const segment_selector_t tr = read_task_register();

    vmwrite(HOST_CS_SELECTOR, *((uint16_t *) &cs));
    vmwrite(HOST_TR_SELECTOR, *((uint16_t *) &tr));
    vmwrite(HOST_SS_SELECTOR, 0);
    vmwrite(HOST_DS_SELECTOR, 0);
    vmwrite(HOST_ES_SELECTOR, 0);
    vmwrite(HOST_FS_SELECTOR, 0);
    vmwrite(HOST_GS_SELECTOR, 0);
}

extern void asm_vmx_exit_handler(void);

static int
setup_exit_handler(void)
{
    const int nr_handler_stack_pages = 10;
    virt_addr_t stack_bot, exit_handler_rsp_val;
    const virt_addr_t exit_handler_addr = __vaddr(asm_vmx_exit_handler);

    if (!is_paging_canonical(exit_handler_addr)) {
        pr_error("Exit handler function addr is not canonical: %lx", exit_handler_addr);
        return -1;
    }

    vmwrite(HOST_RIP, exit_handler_addr);

    if (get_pages_zeroed(nr_handler_stack_pages, &stack_bot) != 0) {
        pr_error("Failed to allocate '%ld' pages for the exit handler stack",
                nr_handler_stack_pages
        );
        return -1;
    }

    exit_handler_rsp_val = (stack_bot + (nr_handler_stack_pages * PAGE_SIZE)) - 8;
    if (!is_paging_canonical(exit_handler_rsp_val)) {
        pr_error("exit handler RSP address is not canonical: %lx", exit_handler_rsp_val);
        return -1;
    }

    vmwrite(HOST_RSP, exit_handler_rsp_val);

    return 0;
}

static inline int
__vmx_initialize_host_state(void)
{
    setup_host_ctrl_registers();

    vmwrite(HOST_IA32_EFER, read_efer().raw);

    vmwrite(HOST_IA32_SYSENTER_ESP, 0);
    vmwrite(HOST_IA32_SYSENTER_EIP, 0);

    setup_host_segment_registers();

    vmwrite(HOST_GDTR_BASE, read_gdtr().base);
    vmwrite(HOST_IDTR_BASE, read_idtr().base);
    vmwrite(HOST_TR_BASE, get_current_tss_base());
    vmwrite(HOST_FS_BASE, 0);
    vmwrite(HOST_GS_BASE, 0);

    if (setup_exit_handler() != 0) {
        pr_error("Error configuring the vm exit handler");
        return -1;
    }

    return 0;
}

int
vmx_set_eptp(vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

int
vmx_set_guest_cr0(vcpu_t *vcpu, const cr0_t cr0)
{
    if (!vcpu_has_option_configured(vcpu, VMX_POLICY_UNRESTRICTED_GUEST)) {
        if (cr0.pg && !cr0.pe) {
            pr_error("Guest with CR0.PG must also set CR0.PE");
            return -1;
        }
    }

    ensure_vcpu_current(vcpu);
    vmwrite(GUEST_CR0, cr0.raw);
    clear_vcpu(vcpu);
    return 0;
}

void
vmx_set_guest_cr3(vcpu_t *vcpu, const cr3_t cr3)
{
    ensure_vcpu_current(vcpu);
    vmwrite(GUEST_CR3, cr3.cr3_64b.raw);
    clear_vcpu(vcpu);
}

void
vmx_set_guest_cr4(vcpu_t *vcpu, const cr4_t cr4)
{
    ensure_vcpu_current(vcpu);
    vmwrite(GUEST_CR4, cr4.raw);
    clear_vcpu(vcpu);
}

int
vmx_set_system_table(
        vcpu_t *vcpu,
        const enum x86_sys_table type,
        struct vcpu_sys_table table
) {
    table.limit &= 0xFFF;

    ensure_vcpu_current(vcpu);

    switch (type) {
        case X86_GDT:
            vmwrite(GUEST_GDTR_BASE, table.base);
            vmwrite(GUEST_GDTR_LIMIT, table.limit);
            goto success_out;
        case X86_IDT:
            vmwrite(GUEST_IDTR_BASE, table.base);
            vmwrite(GUEST_IDTR_LIMIT, table.limit);
            goto success_out;
        default:
            pr_error("Unknown sys table type");
            clear_vcpu(vcpu);
            return -1;
    }

success_out:
    clear_vcpu(vcpu);
    return 0;
}

#define WRITE_GUEST_SEGMENT_REG(type, segment)                          \
    vmwrite(GUEST_ ## type ## _SELECTOR, segment.raw_selector);         \
    vmwrite(GUEST_ ## type ## _BASE, segment.base);                     \
    vmwrite(GUEST_ ## type ## _LIMIT, segment.limit);                   \
    vmwrite(GUEST_ ## type ## _ACCESS_RIGHTS, segment.access_rights.raw)

static int
vmx_set_cs_register(vcpu_t *vcpu, const struct vcpu_segment segment)
{
    NOT_YET_IMPLEMENTED;
}

static int
__vmx_set_ldtr_segment_register(const struct vcpu_segment segment)
{
    if (segment.access_rights.segment_unusable) {
        goto out;
    }

    if (segment.selector.ti) {
        pr_error("LDT descriptor must be in the GDT (i.e. TI flag 0). "
                "But instead TI = %lu",
                U64(segment.selector.ti)
        );
        return -1;
    }

    if (!is_paging_canonical(segment.base)) {
        pr_error("LDT base address is not canonical");
        return -1;
    }

    if (segment.access_rights.segment_type != IA32E_LDT) {
        pr_error("LDTR segment type is not '2' (LDT). Instead: segment_type = %lu",
                U64(segment.access_rights.segment_type)
        );
        return -1;
    }

    if (segment.access_rights.descriptor_type != SYSTEM_SEGMENT_DESC) {
        pr_error("LDTR descriptor type must be 0 (system descriptor). Instead: %lu",
                U64(segment.access_rights.descriptor_type)
        );
        return -1;
    }

    if (segment.access_rights.present != SEGMENT_PRESENT) {
        pr_error("LDT 'present' flag is not set");
        return -1;
    }

    if (segment.access_rights.reserved0 || segment.access_rights.reserved1) {
        pr_error("LDTR: Not all reserved bits are 0");
        return -1;
    }

    if (((segment.limit & 0xFFFF) != 0xFFF) && segment.access_rights.g != 0) {
        pr_error("LDTR: 'g' flag is not 0");
        return -1;
    } else if ((segment.limit >> 20) && segment.access_rights.g != 1) {
        pr_error("LDTR: 'g' flag is not 1");
        return -1;
    }


out:
    WRITE_GUEST_SEGMENT_REG(LDTR, segment);
    return 0;
}

int
vmx_set_segment_register(
        vcpu_t *vcpu,
        const enum x86_segment_reg reg,
        const struct vcpu_segment segment
) {
    ensure_vcpu_current(vcpu);

    /* TODO: add remaining validation */

    switch (reg) {
        case X86_CS_REG:

            if (segment.base >> 32) {
                pr_error("Upper 32 bits of the cs base must be 0");
                return -1;
            }

            WRITE_GUEST_SEGMENT_REG(CS, segment);
            break;
        case X86_SS_REG:
            WRITE_GUEST_SEGMENT_REG(SS, segment);
            break;
        case X86_DS_REG:
            WRITE_GUEST_SEGMENT_REG(DS, segment);
            break;
        case X86_ES_REG:
            WRITE_GUEST_SEGMENT_REG(ES, segment);
            break;
        case X86_FS_REG:
            WRITE_GUEST_SEGMENT_REG(FS, segment);
            break;
        case X86_GS_REG:
            WRITE_GUEST_SEGMENT_REG(GS, segment);
            break;
        case X86_LDTR_REG:
            if (__vmx_set_ldtr_segment_register(segment) != 0) {
                goto error_out;
            }
            break;
        case X86_TR_REG:
            WRITE_GUEST_SEGMENT_REG(TR, segment);
            break;
        default:
            pr_error("Unknown segment type specified");
            clear_vcpu(vcpu);
            return -1;
    }

    clear_vcpu(vcpu);
    return 0;

error_out:
    clear_vcpu(vcpu);
    return -1;
}

/* Targeted vmcs area must be current */
static inline void
__vmx_update_ctrl_vectors(const struct vmx_virt_policy *policy)
{
    vmwrite(PIN_BASED_VM_EXECUTION_CONTROLS, policy->pin_ctls.raw);
    vmwrite(PRIMARY_PROC_BASED_VM_EXEC_CONTROLS, policy->proc_ctls1.raw);
    if (policy->proc_ctls1.activate_secondary_controls) {
        vmwrite(SECONDARY_PROC_BASED_VM_EXEC_CONTROLS, policy->proc_ctls2.raw);
    }
    if (policy->proc_ctls1.activate_tertiary_controls) {
        vmwrite(TERT_PROC_BASED_VM_EXEC_CONTROLS, policy->proc_ctls3.raw);
    }

    vmwrite(PRIMARY_VM_EXIT_CONTROLS, policy->exit_ctls1.raw);
    if (policy->exit_ctls1.activate_secondary_controls) {
        vmwrite(SECONDARY_VM_EXIT_CONTROLS, policy->exit_ctls2.raw);
    }

    vmwrite(VM_ENTRY_CONTROLS, policy->entry_ctls.raw);
}

/**
 * Here we do some basic initialization of the environment that is part of the
 * grander vm and not just the single vcpu (e.g. policy, bitmaps, eptp, etc.)
 * and also some generic defaults for the new vmcs (e.g. activity status, etc.).
 *
 * More finer grained setup is done later with explicit functions.
 */
int
vmx_initialize_vmcs_area(vcpu_t *vcpu)
{
    int ret;
    struct vmx_virt_policy *policy;

    if ((ret = ensure_vcpu_current(vcpu)) != VM_OP_STATUS_SUCCESS) {
        pr_error("Error making vcpu current to initialize VMCS: %lu", U64(ret));
        return -1;
    }

    policy = vcpu->arch.hw.vmx.virt_policy;
    __vmx_update_ctrl_vectors(policy);

    if ((ret = __vmx_initialize_host_state()) != 0) {
        pr_error("Failed to initialize vmcs host state area");
        goto error_out;
    }

    vmwrite(GUEST_ACTIVITY_STATE, GUEST_ACTIVE);
    vmwrite(GUEST_INTERRUPTIBILITY_STATE, 0);
    vmwrite(GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
    vmwrite(VMCS_LINK_POINTER, NO_VMCS_LINK_PTR);

    vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);
    vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
    vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);

    vmwrite(EXCEPTION_BITMAP, 0);
    vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
    vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

    if (vmx_policy_is_option_configured(policy, VMX_POLICY_MSR_BITMAP)) {
        /* TODO: init a bitmap that inhibits vm-exits for every msr */
    }

    if (vmx_policy_is_option_configured(policy, VMX_POLICY_EPT)) {
        vmwrite(EPT_POINTER, vcpu->vm->arch_vm.vmx.eptp.raw);
    }

    vmwrite(GUEST_IA32_SYSENTER_CS, 0);
    vmwrite(GUEST_IA32_SYSENTER_ESP, 0);
    vmwrite(GUEST_IA32_SYSENTER_EIP, 0);
    vmwrite(GUEST_IA32_DEBUGCTL, 0);
    vmwrite(GUEST_DR7, 0);

    clear_vcpu(vcpu);
    return 0;

error_out:
    clear_vcpu(vcpu);
    return ret;
}

extern int do_vmx_vcpu_enter(struct vcpu_user_regs regs);

static int
__vmx_entry_helper(vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

int
vmx_enter_vcpu(vcpu_t *vcpu)
{
    int ret;

    ensure_vcpu_current(vcpu);

    vmwrite(GUEST_RIP, vcpu->arch.state.user_regs.rip);
    vmwrite(GUEST_RSP, vcpu->arch.state.user_regs.rsp);
    vmwrite(GUEST_RFLAGS, vcpu->arch.state.user_regs.rflags);

    ret = do_vmx_vcpu_enter(vcpu->arch.state.user_regs);
    pr_error("Failed to enter VCPU: %lu", U64(ret));
    return ret;
}

/* TODO: move this function to vmcs.c instead */
static void __dump_vmcs(void);

void
vmx_vm_exit_handler(void)
{
    uint64_t ret;
    uint64_t exit_reason_raw;
    vmcs_exit_reason_t exit_reason;

    pr_debug("vmx_vm_exit_handler()");

    if ((ret = vmread(EXIT_REASON, &exit_reason_raw)) != VM_OP_STATUS_SUCCESS) {
        pr_error("Failed to read the vmcs EXIT_REASON field: %lu", ret);
        die();
    }

    exit_reason.raw = exit_reason_raw;

    switch (exit_reason.basic_exit_reason) {
        default:
            __dump_vmcs();
            die();
    }

    die_reason("Unreachable");
}

#define VMCS_DUMP_SECTION_HDR(title) pr_debug("\n***** " #title " *****\n");

#undef DUMP
#define DUMP(enc)                                                       \
    do {                                                                \
        uint64_t val;                                                   \
        int ret = vmread((enc), &val);                                  \
        if (ret != VM_OP_STATUS_SUCCESS) {                              \
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
__dump_vmcs(void)
{
    uint64_t ret = 0;
    uint64_t exit_reason_raw = 0;
    vmcs_exit_reason_t exit_reason;
    uint64_t exit_qualification_raw = 0;
    union vmcs_exit_qualification exit_qualification;

    pr_debug("\n ******** VMCS DUMP START ********\n");

    VMCS_DUMP_SECTION_HDR("VM-exit information");

    if ((ret = vmread(EXIT_REASON, &exit_reason_raw)) != VM_OP_STATUS_SUCCESS) {
        __printf("VMREAD failed for EXIT_REASON: %lu\n", ret);
    } else {
        exit_reason.raw = exit_reason_raw;
        __printf("EXIT_REASON_RAW = 0x%lx\n", exit_reason_raw);
        __printf("BASIC_EXIT_REASON = 0x%lx\n", U64(exit_reason.basic_exit_reason));
        __printf("VM_ENTRY_FAILURE = 0x%lx\n", U64(exit_reason.vm_entry_failure));
    }

    /* TODO: add specific functions for displaying e.g. ept violation qualification, etc. */
    if ((ret = vmread(EXIT_QUALIFICATION, &exit_qualification_raw)) != VM_OP_STATUS_SUCCESS) {
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

    DUMP(ADDRESS_MSRR_BITMAPS);

    DUMP(VIRTUAL_APIC_ADDRESS);
    DUMP(APIC_ACCESS_ADDRESS);

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

