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

    set_percpu_val(curr_vcpu, vcpu);
    return 0;
}

int
vmx_vcpu_allocate(vcpu_t *vcpu)
{
    int res;
    phys_addr_t vmcs_ptr;

    vmcs_ptr = create_new_vmcs_area();
    if (!vmcs_ptr) {
        pr_error("vcpu initialization failed due to the failure of creating a new vmcs area");
        return -1;
    }

    res = vmclear(vmcs_ptr);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_error("'vmclear' on new vmcs area failed: %lu", U64(res));
        free_page_raw(vmcs_ptr);
        return -1;
    }

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

static inline void
setup_host_system_tables(void)
{
    vmwrite(HOST_GDTR_BASE, read_gdtr().base);
    vmwrite(HOST_IDTR_BASE, read_idtr().base);
    vmwrite(HOST_TR_BASE, get_current_tss_base());
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

static int
__vmx_initialize_host_state(vcpu_t *vcpu)
{
    int ret;

    setup_host_ctrl_registers();

    vmwrite(HOST_IA32_SYSENTER_ESP, 0);
    vmwrite(HOST_IA32_SYSENTER_EIP, 0);

    setup_host_segment_registers();

    vmwrite(HOST_FS_BASE, 0);
    vmwrite(HOST_GS_BASE, 0);

    setup_host_system_tables();

    if ((ret = setup_exit_handler()) != 0) {
        pr_error("Error configuring the vm exit handler");
        return ret;
    }

    return 0;
}

static int
validate_eptp_configuration(const vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

int
validate_vcpu_configuration(const vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

int
configure_vmcs_from_vcpu(vcpu_t *vcpu)
{
    int ret;

    /* todo */

    ret = vmx_configure_host_state(vcpu);
    if (ret != 0) {
        pr_error("Error configuring the host state in the VMCS area");
        return ret;
    }

    /* todo */

    NOT_YET_IMPLEMENTED;

    return 0;
}

