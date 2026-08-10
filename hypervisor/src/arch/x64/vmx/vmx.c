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
#include "vmx/vmx_ops.h"

DEFINE_PER_CPU(vcpu_t *, curr_vcpu);
#define current_vcpu (percpu_val(curr_vcpu))

static int
ensure_vcpu_current(vcpu_t *vcpu)
{
    if (vcpu == current_vcpu) {
        return 0;
    }

    if (vcpu->arch.active && !(vcpu->arch.active_processor == get_current_cpuid())) {
        pr_warn("Cannot make vcpu current as its already current on another processor!"
                "Current processor (%lu) vs. processor the vcpu is current on (%lu)",
                U64(get_current_cpuid()),
                U64(vcpu->arch.active_processor)
        );

        return -1;
    }

    const int res = vmptrld(vcpu->arch.hw.vmx.vmcs_ptr);
    if (res != VMX_OP_SUCCESS) {
        pr_warn("'vmptrld' in ensure_vcpu_current() has failed: %lu", res);
        return res;
    }

    vcpu->arch.active = true;
    vcpu->arch.active_processor = get_current_cpuid();

    set_percpu_val(curr_vcpu, vcpu);
    return 0;
}

static inline int
clear_vcpu(vcpu_t *vcpu)
{
    if (vcpu->arch.active && !(vcpu->arch.active_processor == get_current_cpuid())) {
        pr_warn("Cannot clear a VMCS that is active on another processor! "
                "Current processor (%lu) vs. processor the VMCS is active on (%lu)",
                U64(get_current_cpuid()),
                U64(vcpu->arch.active_processor)
        );
        return -1;
    }

    if (!vcpu->arch.active) {
        pr_debug("Clearing an already non-active vcpu");
    }

    vmclear(vcpu->arch.hw.vmx.vmcs_ptr);

    if (vcpu == current_vcpu) {
        current_vcpu = NULL;
    }

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
    if (res != VMX_OP_SUCCESS) {
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
    phys_addr_t *curr_vmcs_ptr = &vcpu->arch.hw.vmx.vmcs_ptr;

    if (!vcpu->arch.active) {
        goto do_destroy;
    }

    if (vcpu->arch.active_processor == get_current_cpuid()) {
        clear_vcpu(vcpu);
        goto do_destroy;
    } else {
        pr_info("Need to execute 'clear_vcpu' on the target procesor (%lu)!",
                U64(vcpu->arch.active_processor)
        );
        NOT_YET_IMPLEMENTED;
    }

do_destroy:

    if (*curr_vmcs_ptr) {
        free_page_raw(*curr_vmcs_ptr);
        *curr_vmcs_ptr = 0;
    }
}

#define VMX_CR0_FIXED0 (read_msr(MSRX64_IA32_VMX_CR0_FIXED0))
#define VMX_CR0_FIXED1 (read_msr(MSRX64_IA32_VMX_CR0_FIXED1))
#define VMX_CR4_FIXED0 (read_msr(MSRX64_IA32_VMX_CR4_FIXED0))
#define VMX_CR4_FIXED1 (read_msr(MSRX64_IA32_VMX_CR4_FIXED1))

cr0_t
sanitize_cr0_for_vmx_operation(const cr0_t cr0)
{
    cr0_t sanitized;
    sanitized.raw = (cr0.raw | VMX_CR0_FIXED0) & VMX_CR0_FIXED1;
    return sanitized;
}

cr3_t
sanitize_cr3_for_vmx_operation(cr3_t cr3)
{
    cr3.cr3_64b.ignored0 = 0;
    cr3.cr3_64b.ignored1 = 0;
    cr3.cr3_64b.reserved0 = 0;
    cr3.cr3_64b.reserved1 = 0;
    return cr3;
}

cr4_t
sanitize_cr4_for_vmx_operation(const cr4_t cr4)
{
    cr4_t sanitized;
    sanitized.raw = (cr4.raw | VMX_CR4_FIXED0) & VMX_CR4_FIXED1;
    return sanitized;
}

static inline void
sanitize_ctrl_regs_for_vmxon(void)
{
    cr0_t cr0;
    cr4_t cr4;

    cr0.raw = read_cr0();
    cr0 = sanitize_cr0_for_vmx_operation(cr0);
    write_cr0(cr0.raw);

    cr4.raw = read_cr4();
    cr4 = sanitize_cr4_for_vmx_operation(cr4);
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
        pr_error("Failed to initialize the vmx capability info: %lu", U64(res));
        return false;
    }

    vmxon_region = create_vmxon_region();
    if (!vmxon_region) {
        pr_error("Unable to enter VMX operation: Creating a VMXON region failed");
        return false;
    }

    sanitize_ctrl_regs_for_vmxon();

    res = vmxon(vmxon_region);
    if (res != VMX_OP_SUCCESS) {
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
    cr0 = sanitize_cr0_for_vmx_operation(cr0);
    vmwrite(HOST_CR0, cr0.raw);

    cr3.cr3_64b.raw = read_cr3();
    cr3 = sanitize_cr3_for_vmx_operation(cr3);
    vmwrite(HOST_CR3, cr3.cr3_64b.raw);

    cr4.raw = read_cr4();
    cr4.pae = 1;
    cr4 = sanitize_cr4_for_vmx_operation(cr4);
    vmwrite(HOST_CR4, cr4.raw);
}

static inline void
setup_host_segment_registers(void)
{
    const segment_selector_t cs = read_segment_register(X86_CS_REG);
    const segment_selector_t tr = read_segment_register(X86_TR_REG);

    vmwrite(HOST_CS_SELECTOR, *((uint16_t *) &cs));
    vmwrite(HOST_TR_SELECTOR, *((uint16_t *) &tr));
    vmwrite(HOST_SS_SELECTOR, 0);
    vmwrite(HOST_DS_SELECTOR, 0);
    vmwrite(HOST_ES_SELECTOR, 0);
    vmwrite(HOST_FS_SELECTOR, 0);
    vmwrite(HOST_GS_SELECTOR, 0);
}

extern void asm_vmx_vm_exit_handler(struct vcpu_user_regs *regs);

static int
setup_exit_handler(void)
{
    const vcpu_t *vcpu = current_vcpu;
    const struct vcpu_user_regs *regs = &vcpu->arch.state.user_regs;
    const int nr_handler_stack_pages = 10;
    virt_addr_t stack_bot, exit_handler_rsp_val;
    const virt_addr_t exit_handler_addr = __vaddr(asm_vmx_vm_exit_handler);

    if (!is_paging_canonical(exit_handler_addr)) {
        pr_error("Exit handler function addr is not canonical: 0x%lx", exit_handler_addr);
        return -1;
    }

    vmwrite(HOST_RIP, exit_handler_addr);

    if (get_pages_zeroed(nr_handler_stack_pages, &stack_bot) != 0) {
        pr_error("Failed to allocate '%ld' pages for the exit handler stack",
                nr_handler_stack_pages
        );
        return -1;
    }

    /**
     * We store pointers to the vcpu struct and the user regs struct right above
     * the rsp so that we can save user regs etc. on a vm exit. I.e.
     *  | vcpu ptr |
     *  ------------
     *  | regs ptr |
     *  ------------ <-- host rsp that we write into the vmcs.
     */
    exit_handler_rsp_val = (stack_bot + (nr_handler_stack_pages * PAGE_SIZE)) - 8;
    memcpy((void *) exit_handler_rsp_val, &vcpu, 8);

    exit_handler_rsp_val -= 8;
    memcpy((void *) exit_handler_rsp_val, &regs, 8);

    if (!is_paging_canonical(exit_handler_rsp_val)) {
        pr_error("exit handler RSP is not canonical: 0x%lx", exit_handler_rsp_val);
        free_pages(nr_handler_stack_pages, stack_bot);
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

void
vmx_set_guest_efer(vcpu_t *vcpu, const ia32_efer_t efer)
{
    ensure_vcpu_current(vcpu);
    vmwrite(GUEST_IA32_EFER, efer.raw);
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

    if ((ret = ensure_vcpu_current(vcpu)) != VMX_OP_SUCCESS) {
        pr_error("Error making vcpu current to initialize VMCS: %lu", U64(ret));
        return -1;
    }

    policy = vcpu->arch.hw.vmx.virt_policy;
    __vmx_update_ctrl_vectors(policy);

    if (__vmx_initialize_host_state() != 0) {
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
        if (vcpu->vm->arch_vm.vmx.msr_bitmap_addr) {
            vmwrite(ADDRESS_MSR_BITMAPS, vcpu->vm->arch_vm.vmx.msr_bitmap_addr);
        } else  {
            pr_warn("MSR bitmap config is configured but no msr bitmap address "
                    "is provided. Skipping this field in the VMCS initialization"
            );
        }
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

/* in vmx/entry.S */
extern int asm_do_vmx_vcpu_entry(
        const vcpu_t *vcpu,
        const enum vmcs_launch_state launch_state,
        const struct vcpu_user_regs *regs
);

bool
__vmx_entry_helper(vcpu_t *vcpu)
{
    vmwrite(GUEST_RIP, vcpu->arch.state.user_regs.rip);
    vmwrite(GUEST_RSP, vcpu->arch.state.user_regs.rsp);
    vmwrite(GUEST_RFLAGS, vcpu->arch.state.user_regs.rflags);
    return true;
}

int
vmx_enter_vcpu(vcpu_t *vcpu)
{
    int ret;
    enum vmcs_launch_state launch_state = vcpu->arch.hw.vmx.launch_state;

    if ((ret = ensure_vcpu_current(vcpu)) != 0) {
        pr_error("vCPU entry failed due to error making the vcpu current: %s",
                stringify_vmx_op_status(ret)
        );
        return -1;
    }

    if (launch_state == VMCS_LS_INVALID) {
        pr_error("vCPU launch state is invalid (neither 'clear' or 'launched')");
        return -1;
    }

    ret = asm_do_vmx_vcpu_entry(vcpu, launch_state, &vcpu->arch.state.user_regs);
    pr_error("vmx entry op unsucessful: %s", stringify_vmx_op_status(ret));
    dump_vmcs();
    die_reason("blocker for now");
}

void
vmx_vm_exit_handler(struct vcpu_user_regs *regs)
{
    uint64_t exit_reason_raw;
    vmcs_exit_reason_t exit_reason;

    vmread(GUEST_RIP, &regs->rip);
    vmread(GUEST_RSP, &regs->rsp);
    vmread(GUEST_RFLAGS, &regs->rflags);

    current_vcpu->arch.hw.vmx.launch_state = VMCS_LS_LAUNCHED;

    vmread(EXIT_REASON, &exit_reason_raw);
    exit_reason.raw = exit_reason_raw;

    /**
     * TODO: Create an emlation struct with function pointers to actual handlers,
     * e.g. vcpu->emulate.cpuid(), and default impls.
     */
    switch (exit_reason.basic_exit_reason) {
        case EXIT_REASON_CPUID:
            pr_info("handling cpuid exit");
            NOT_YET_IMPLEMENTED;
            /*regs->rip += 1;
            vmx_enter_vcpu(current_vcpu);*/
        default:
            dump_vmcs();
            die();
    }

    die_reason("Unreachable");
}

vmx_op_status_t
eflags_to_vmx_op_status(const uint32_t flags)
{
    return __check_vm_op_status(flags);
}

