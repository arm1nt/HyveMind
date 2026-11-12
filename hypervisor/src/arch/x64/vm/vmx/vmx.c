#include "fatal.h"
#include "string.h"
#include "per-cpu.h"
#include "pf_alloc.h"
#include "printf.h"
#include "vm.h"
#include "asm/cpufeatures.h"
#include "asm/gdt_idt.h"
#include "asm/irq_vectors.h"
#include "asm/paging.h"
#include "asm/processor.h"
#include "asm/vmm.h"
#include "vm/io.h"
#include "vm/vcpu.h"
#include "vmx/policy.h"
#include "vmx/vmx.h"
#include "vmx/vmcs.h"
#include "vmx/vmx_ops.h"

DEFINE_PER_CPU(vcpu_t *, curr_vcpu);
#define current_vcpu (percpu_val(curr_vcpu))

void
__vmx_clear_vcpu(vcpu_t *vcpu)
{
    if (vcpu_vmx(vcpu).active_cpu != get_current_cpuid()) {
        pr_warn("Cannot clear vCPU that is active on cpu %lu on cpu %lu",
                vcpu_vmx(vcpu).active_cpu,
                get_current_cpuid()
        );
        return;
    }

    vmclear(vcpu_vmx(vcpu).vmcs_ptr);

    vcpu_vmx(vcpu).active_cpu = INVALID_CPUID;
    vcpu_vmx(vcpu).launch_state = VMCS_LS_CLEAR;

    if (vcpu == current_vcpu) {
        set_percpu_val(curr_vcpu, 0);
    }
}

void
vmx_clear_vcpu(vcpu_t *vcpu)
{
    if (vcpu_vmx(vcpu).active_cpu == INVALID_CPUID)
        return;

    if (vcpu_vmx(vcpu).active_cpu == get_current_cpuid()) {
        __vmx_clear_vcpu(vcpu);
        return;
    }

    /**
     * Todo:
     *  Send IPI to the processor on which the vCPU is active and have it
     *  call __vmx_clear_vcpu on the vCPU.
     */
    NOT_YET_IMPLEMENTED;
}

static int
vmx_load_vcpu(vcpu_t *vcpu)
{
    const int res = vmptrld(vcpu_vmx(vcpu).vmcs_ptr);
    if (res != VMX_SUCCESS) {
        pr_warn("Failed to load vCPU");
        return res;
    }

    vcpu_vmx(vcpu).active_cpu = get_current_cpuid();
    return 0;
}

static int
vmx_make_vcpu_current(vcpu_t *vcpu)
{
    if (vcpu == current_vcpu)
        return 0;

    if (vcpu_vmx(vcpu).active_cpu == INVALID_CPUID)
        vcpu_vmx(vcpu).active_cpu = get_current_cpuid();

    if (vcpu_vmx(vcpu).active_cpu != get_current_cpuid()) {
        pr_debug("Making vCPU active on cpu %lu current on cpu %lu",
                vcpu_vmx(vcpu).active_cpu,
                get_current_cpuid()
        );

        vmx_clear_vcpu(vcpu);
    }

    const int res = vmx_load_vcpu(vcpu);
    if (res != 0) {
        vcpu_vmx(vcpu).active_cpu = INVALID_CPUID;
        return res;
    }

    set_percpu_val(curr_vcpu, vcpu);
    return 0;
}

/**
 * Before calling, it must be ensured that the vCPU is not currently running on
 * any cpu.
 */
void
vmx_destroy_vcpu(vcpu_t *vcpu)
{
    vmx_clear_vcpu(vcpu);

    free_page_raw(vcpu_vmx(vcpu).vmcs_ptr);
    vcpu_vmx(vcpu).vmcs_ptr = 0;
}

int
vmx_vcpu_allocate(vcpu_t *vcpu)
{
    int res;
    phys_addr_t vmcs_ptr;

    vmcs_ptr = create_new_vmcs_area();
    if (!vmcs_ptr) {
        pr_error("Failed to allocate a VMCS area");
        return -ERR_NO_MEM;
    }

    res = vmclear(vmcs_ptr);
    if (res != VMX_SUCCESS) {
        pr_error("'vmclear' on new VMCS area failed: %lu", U64(res));
        free_page_raw(vmcs_ptr);
        return -1;
    }

    vcpu_vmx(vcpu).vmcs_ptr = vmcs_ptr;
    vcpu_vmx(vcpu).launch_state = VMCS_LS_CLEAR;
    vcpu_vmx(vcpu).active_cpu = INVALID_CPUID;

    return 0;
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
    if (res != VMX_SUCCESS) {
        pr_error("Executing 'vmxon' failed with error status: %lu", U64(res));
        free_page_raw(vmxon_region);
        return false;
    }

    pr_info("Successfully entered VMX operation");
    current_cpu->vmxon_region_ptr = vmxon_region;

    return true;
}

void
leave_vmx_operation(void)
{
    if (!current_cpu->vmxon_region_ptr) {
        pr_warn("Cannot execute 'vmxoff' since logical processor is not in VMX operation");
        return;
    }

    vmxoff();

    free_page_raw(current_cpu->vmxon_region_ptr);
    current_cpu->vmxon_region_ptr = 0;

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
    cr4.osxsave = 1;
    cr4 = sanitize_cr4_for_vmx_operation(cr4);
    vmwrite(HOST_CR4, cr4.raw);
}

static inline void
setup_host_segment_registers(void)
{
    const segment_selector_t cs = read_segment_register(X86_CS_REG);

    vmwrite(HOST_CS_SELECTOR, *((uint16_t *) &cs));
    vmwrite(HOST_SS_SELECTOR, 0);
    vmwrite(HOST_DS_SELECTOR, 0);
    vmwrite(HOST_ES_SELECTOR, 0);
    vmwrite(HOST_FS_SELECTOR, 0);
    vmwrite(HOST_GS_SELECTOR, 0);
}

extern void asm_vmx_vm_exit_handler(void);

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

    vmx_make_vcpu_current(vcpu);
    vmwrite(GUEST_CR0, cr0.raw);
    return 0;
}

void
vmx_set_guest_cr3(vcpu_t *vcpu, const cr3_t cr3)
{
    vmx_make_vcpu_current(vcpu);
    vmwrite(GUEST_CR3, cr3.cr3_64b.raw);
}

void
vmx_set_guest_cr4(vcpu_t *vcpu, const cr4_t cr4)
{
    vmx_make_vcpu_current(vcpu);
    vmwrite(GUEST_CR4, cr4.raw);
}

void
vmx_set_guest_efer(vcpu_t *vcpu, const ia32_efer_t efer)
{
    vmx_make_vcpu_current(vcpu);
    vmwrite(GUEST_IA32_EFER, efer.raw);
}

void
vmx_get_default_cr4_mask_and_shadow(uint64_t *mask, uint64_t *shadow)
{
    /**
     * The mask sets all bits that are fixed by vmx-operation as host owned.
     * In the shadow, we set all host owned bits to the default-required values.
     */
    const uint64_t vmx_flexible_cr4_bits = VMX_CR4_FIXED0 ^ VMX_CR4_FIXED1;
    const uint64_t vmx_fixed_cr4_bits = ~vmx_flexible_cr4_bits;

    *mask = vmx_fixed_cr4_bits;
    *shadow = VMX_CR4_FIXED0 & VMX_CR4_FIXED1;
}

void
vmx_set_cr4_mask_and_shadow(vcpu_t *vcpu, const uint64_t mask, const uint64_t shadow)
{
    vmx_make_vcpu_current(vcpu);
    vmwrite(CR4_GUEST_HOST_MASK, mask);
    vmwrite(CR4_READ_SHADOW, shadow);
}

int
vmx_set_system_table(vcpu_t *vcpu, const enum x86_sys_table type, struct vcpu_sys_table table)
{
    table.limit &= 0xFFF;
    vmx_make_vcpu_current(vcpu);

    switch (type) {
        case X86_GDT:
            vmwrite(GUEST_GDTR_BASE, table.base);
            vmwrite(GUEST_GDTR_LIMIT, table.limit);
            break;
        case X86_IDT:
            vmwrite(GUEST_IDTR_BASE, table.base);
            vmwrite(GUEST_IDTR_LIMIT, table.limit);
            break;
    }

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
    if (segment.access_rights.segment_unusable)
        goto out;

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
    vmx_make_vcpu_current(vcpu);
    /* TODO: add remaining validation */

    switch (reg) {
        case X86_CS_REG:
            if (segment.base >> 32) {
                pr_error("Upper 32 bits of the cs base must be 0");
                goto error_out;
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
            return -1;
    }

    return 0;
error_out:
    return -1;
}

/**
 * Todo:
 *  Refactor to use generic bitmap handling logic instead of (re-)implementing
 *  it here.
 */
static inline void
__modify_msr_bitmap_bit(uint8_t *bitmap, const uint64_t sanitized_msr, const bool set)
{
    const unsigned int byte_pos = sanitized_msr / 8;
    const unsigned int bit_pos = sanitized_msr % 8;

    uint8_t *byte = &bitmap[byte_pos];

    if (set) {
        *byte |= U8_LSHIFT(1, bit_pos);
    } else {
        *byte &= ~(U8_LSHIFT(1, bit_pos));
    }
}

void
vmx_set_intercept_reg_rdmsr(const struct vm *vm, const uint64_t msr, const bool intercept)
{
    uint64_t sanitized_msr = msr;
    uint8_t *bitmap_base = (uint8_t *) phys_to_virt(vm->arch_vm.vmx.msr_bitmap_addr);

    if (__is_low_vmcs_msr(msr)) {
        goto do_mod_bit;
    } else if (__is_high_vmcs_msr(msr)) {
        bitmap_base += VMCS_HIGH_READ_MSR_OFFSET;
        sanitized_msr -= VMCS_MSR_HIGH_START;
        goto do_mod_bit;
    }

    pr_error("Attempted to change read interceptability status of invalid msr: %lu", msr);
    die();

do_mod_bit:
    __modify_msr_bitmap_bit(bitmap_base, sanitized_msr, intercept);
}

void
vmx_set_intercept_reg_wrmsr(const struct vm *vm, const uint64_t msr, const bool intercept)
{
    uint64_t sanitized_msr = msr;
    uint8_t *bitmap_base = (uint8_t *) phys_to_virt(vm->arch_vm.vmx.msr_bitmap_addr);

    if (__is_low_vmcs_msr(msr)) {
        bitmap_base += VMCS_LOW_WRITE_MSR_OFFSET;
        goto do_mod_bit;
    } else if (__is_high_vmcs_msr(msr)) {
        bitmap_base += VMCS_HIGH_WRITE_MSR_OFFSET;
        sanitized_msr -= VMCS_MSR_HIGH_START;
        goto do_mod_bit;
    }

    pr_error("Attempted to change write interceptability status of invalid msr: %lu", msr);
    die();

do_mod_bit:
    __modify_msr_bitmap_bit(bitmap_base, sanitized_msr, intercept);
}

void
vmx_set_intercept_msr(const struct vm *vm, const uint64_t msr, const bool intercept)
{
    vmx_set_intercept_reg_rdmsr(vm, msr, intercept);
    vmx_set_intercept_reg_wrmsr(vm, msr, intercept);
}

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
    cr4_t cr4_mask, cr4_shadow;

    if ((ret = vmx_make_vcpu_current(vcpu)) != VMX_SUCCESS) {
        pr_error("Error making vcpu current to initialize VMCS: %lu", U64(ret));
        return -1;
    }

    policy = vcpu_virt_policy(vcpu);
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

    if (vmx_policy_is_option_configured(policy, VMX_POLICY_IO_BITMAP)) {
        const phys_addr_t bitmap_a = vcpu->vm->arch_vm.vmx.io_bitmap_addr;
        const phys_addr_t bitmap_b = bitmap_a + VMCS_HIGH_IO_OFFSET;
        vmwrite(ADDRESS_IO_BITMAP_A,bitmap_a);
        vmwrite(ADDRESS_IO_BITMAP_B, bitmap_b);
    }

    if (vmx_policy_is_option_configured(policy, VMX_POLICY_EPT)) {
        vmwrite(EPT_POINTER, vcpu->vm->arch_vm.vmx.eptp.raw);
    }

    if (vmx_policy_is_option_configured(policy, __VMX_POLICY_VIRTUALIZE_APIC)) {
        vmwrite(APIC_ACCESS_PAGE, vm_apic_access_page(vcpu->vm));
        vmwrite(VIRTUAL_APIC_PAGE, vcpu_vlapic(vcpu)->virtual_apic_page);
        vmwrite(TPR_TRESHOLD, 0);
    }

    vmx_get_default_cr4_mask_and_shadow(&cr4_mask.raw, &cr4_shadow.raw);
    /* Hide that vmx is enabled */
    cr4_shadow.vmxe = 0;
    vmwrite(CR4_GUEST_HOST_MASK, cr4_mask.raw);
    vmwrite(CR4_READ_SHADOW, cr4_shadow.raw);

    vmwrite(GUEST_IA32_SYSENTER_CS, 0);
    vmwrite(GUEST_IA32_SYSENTER_ESP, 0);
    vmwrite(GUEST_IA32_SYSENTER_EIP, 0);
    vmwrite(GUEST_IA32_DEBUGCTL, 0);
    vmwrite(GUEST_DR7, 0);

    return 0;
error_out:
    return ret;
}

void
vmx_inject_exception(vcpu_t *vcpu, const int vector, const int error_code)
{
    vmcs_event_inject_t event_id;
    event_id.raw = 0;
    event_id.type = INJECT_EVENT_TYPE_HW_EXCEPTION;
    event_id.valid = 1;
    event_id.vector = vector;

    switch (vector) {
        case IRQ_UD_VECTOR:
            /* No error code */
            break;
        case IRQ_GP_VECTOR:
        default:
            event_id.error_code = 1;
            vmwrite(INJECTED_EVENT_ERROR_CODE, error_code);
            break;
    }

    vmwrite(INJECTED_EVENT_IDENTIFICATION, event_id.raw);
}

static void
vmx_inject_ext_intr(const uint8_t vector)
{
    vmcs_event_inject_t event;
    event.raw = 0;

    event.vector = vector;
    event.type = INJECT_EVENT_TYPE_EXT_INTERRUPT;
    event.valid = 1;
    vmwrite(INJECTED_EVENT_IDENTIFICATION, U64(event.raw));
}

static void
vmx_set_intr_window_exiting_ctrl(const int val)
{
    uint64_t raw_ctrl;
    vmcs_procbased_ctls1 ctls;
    if (vmread(PRIMARY_PROC_BASED_VM_EXEC_CONTROLS, &raw_ctrl) != 0) {
        die_reason("reading proc based ctls1 for intr window failed");
    }
    ctls.raw = U64_LOWER32(raw_ctrl);
    ctls.interrupt_window_exiting = val;
    vmwrite(PRIMARY_PROC_BASED_VM_EXEC_CONTROLS, U64(ctls.raw));
}

void
vmx_update_host_state(const virt_addr_t exit_handler_rsp)
{
    vmwrite(HOST_RSP, exit_handler_rsp);

    const segment_selector_t tr = read_segment_register(X86_TR_REG);
    vmwrite(HOST_TR_SELECTOR, *((uint16_t *) &tr));
    vmwrite(HOST_TR_BASE, get_current_tss_base());
}

int
vmx_prepare_vcpu_execution(vcpu_t *vcpu)
{
    return vmx_make_vcpu_current(vcpu);
}

int
__vmx_entry_helper(vcpu_t *vcpu)
{
    int ret = VMX_SUCCESS;
    ret |= vmwrite(GUEST_RIP, vcpu->arch.state.user_regs.rip);
    ret |= vmwrite(GUEST_RSP, vcpu->arch.state.user_regs.rsp);
    ret |= vmwrite(GUEST_RFLAGS, vcpu->arch.state.user_regs.rflags);

    disable_local_interrupts();
    spinlock_acquire(&vcpu->lock);
    vcpu->state = VCPU_RUNNING;
    spinlock_release(&vcpu->lock);

    return ret;
}

void
vmx_entry_failure_cleanup(vcpu_t *vcpu)
{
    uint64_t flags;
    spinlock_acquire_irq_save(&vcpu->lock, flags);
    vcpu->state = VCPU_NOT_RUNNING;
    spinlock_release_irq_restore(&vcpu->lock, flags);
}

void
vmx_external_irqs_entry_helper(vcpu_t *vcpu)
{
    /* Todo: refactor */
    if (!vcpu_has_external_irq(vcpu))
        return;

    if (!vpic_has_pending_irq(vm_vpic(vcpu->vm)))
        return;

    bool interruptible = true;

    rflags_t rflags;
    rflags.raw = vcpu->arch.state.user_regs.rflags;
    if (rflags.intrf == 0)
        interruptible = false;

    uint64_t act_state = 0;
    vmread(GUEST_ACTIVITY_STATE, &act_state);
    if (act_state)
        interruptible = false;

    uint64_t intr_state = 0;
    vmread(GUEST_INTERRUPTIBILITY_STATE, &intr_state);
    if (U64_LOWER32(intr_state) != 0)
        interruptible = false;

    if (!interruptible) {
        vmx_set_intr_window_exiting_ctrl(1);
    } else {
        const int irq = vcpu_get_external_irq(vcpu);
        vmx_inject_ext_intr(irq);
    }
}

static inline void
dump_vcpu_state(vcpu_t *vcpu)
{
    // Todo: dump vpic and vlapic state as well
    dump_vmcs();
}

static struct pio_operation
vmx_exit_qual_to_pio_op(const uint64_t io_exit_qual_raw)
{
    struct pio_operation pio;
    vmcs_io_exit_qualification_t io_info;
    io_info.raw = io_exit_qual_raw;

    switch (io_info.direction) {
        case IO_QUAL_READ:
            pio.direction = PIO_OP_READ;
            break;
        case IO_QUAL_WRITE:
            pio.direction = PIO_OP_WRITE;
    }

    switch (io_info.access_size) {
        case IO_QUAL_SIZE_BYTE:
            pio.size = PIO_OP_SIZE_BYTE;
            break;
        case IO_QUAL_SIZE_WORD:
            pio.size = PIO_OP_SIZE_WORD;
            break;
        case IO_QUAL_SIZE_DWORD:
            pio.size = PIO_OP_SIZE_DWORD;
    }

    pio.port = io_info.port;
    return pio;
}

int
vmx_vm_exit_handler(vcpu_t *vcpu)
{
    uint64_t exit_reason_raw, exit_qualification_raw;
    vmcs_exit_reason_t exit_reason;
    struct vcpu_user_regs *regs = &vcpu->arch.state.user_regs;

    spinlock_acquire(&vcpu->lock);
    vcpu_vmx(vcpu).launch_state = VMCS_LS_LAUNCHED;
    vcpu->state = VCPU_NOT_RUNNING;
    spinlock_release(&vcpu->lock);

    vmread(GUEST_RIP, &regs->rip);
    vmread(GUEST_RSP, &regs->rsp);
    vmread(GUEST_RFLAGS, &regs->rflags);

    vmread(EXIT_QUALIFICATION, &exit_qualification_raw);
    vmread(EXIT_REASON, &exit_reason_raw);
    exit_reason.raw = exit_reason_raw;

    if (exit_reason.vm_entry_failure) {
        pr_error("VM-entry failed for vCPU of VM %s", vcpu->vm->name);
        dump_vcpu_state(vcpu);
        return VMX_VM_ENTRY_ERROR;
    }

    enable_local_interrupts();

    switch (exit_reason.basic_exit_reason) {
        case EXIT_REASON_EXT_INTR:
            break;
        case EXIT_REASON_INTR_WINDOW:
            vmx_set_intr_window_exiting_ctrl(0);
            break;
        case EXIT_REASON_CPUID:
            vcpu_handle_cpuid(vcpu);
            break;
        case EXIT_REASON_IO_INSTRUCTION:
            struct pio_operation pio = vmx_exit_qual_to_pio_op(exit_qualification_raw);
            handle_guest_io_op(&pio, vcpu);
            vcpu_step_rip(vcpu);
            break;
        case EXIT_REASON_RDMSR_IMPLICIT:
        case EXIT_REASON_RDMSR_IMMEDIATE:
            vcpu_handle_rdmsr(vcpu);
            break;
        case EXIT_REASON_XSETBV:
            vcpu_handle_xsetbv(vcpu);
            break;
        case EXIT_REASON_APIC_WRITE:
            /* Currently we don't take any extra action after a vlapic write */
            break;
        default:
            dump_vcpu_state(vcpu);
            NOT_YET_IMPLEMENTED;
    }

    return VMX_SUCCESS;
}

