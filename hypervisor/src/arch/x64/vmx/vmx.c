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
init_vcpu(vcpu_t *vcpu)
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
clear_vcpu(vcpu_t *vcpu)
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

int
vmx_validate_virt_policy(const vcpu_t *vcpu)
{
    pr_debug("valiidate virt policy");

    return -1;
}

void
vmx_init_default_policy(struct vmx_virt_policy *policy)
{
    pr_info("Init default policy");
}

void
vmx_unpaged_pm_guest_policy(struct vmx_virt_policy *policy)
{
    pr_info("configuring policy for a unpaged pm guest");
}

void
vmx_64bit_mode_guest_policy(struct vmx_virt_policy *policy)
{
    pr_info("Configuring policy for a long mode guest");
}

static inline void
setup_host_ctrl_registers(vcpu_t *vcpu)
{

}

static int
setup_host_cs(void)
{
    const segment_selector_t cs = DEFINE_SEGMENT_SELECTOR(HYVEMIND_CS_SEGMENT_INDEX, TI_GDT, 0);
    const uint16_t cs_raw = *((uint16_t *) & cs);

    if (cs_raw == 0) {
        pr_warn("Host CS selector is 0x00");
        return -1;
    }

    vmwrite(HOST_CS_SELECTOR, cs_raw);
    return 0;
}

static int
setup_host_tr(void)
{
    const segment_selector_t host_tr = read_task_register();
    const uint16_t raw_host_tr = *((uint16_t *) &host_tr);

    if (raw_host_tr == 0) {
        pr_warn("Host TR selector is 0x00");
        return -1;
    }

    vmwrite(HOST_TR_SELECTOR, U64(raw_host_tr));
    return 0;
}

static inline int
setup_host_segment_registers(void)
{
    int ret;

    ret = setup_host_cs();
    if (ret != 0) {
        pr_error("Failed to set cs selector of host");
        return ret;
    }

    ret = setup_host_tr();
    if (ret != 0) {
        pr_error("Failed to set tr selector of host");
        return ret;
    }

    vmwrite(HOST_SS_SELECTOR, 0);
    vmwrite(HOST_DS_SELECTOR, 0);
    vmwrite(HOST_ES_SELECTOR, 0);
    vmwrite(HOST_FS_SELECTOR, 0);
    vmwrite(HOST_GS_SELECTOR, 0);

    return 0;
}

static inline int
setup_host_system_tables(void)
{
    return 0;
}

static int
setup_exit_handler(vcpu_t *vcpu)
{
    return 0;
}

int
vmx_configure_host_state(vcpu_t *vcpu)
{
    int ret;

    ensure_vcpu_current(vcpu);

    setup_host_ctrl_registers(vcpu);

    vmwrite(HOST_IA32_SYSENTER_ESP, 0);
    vmwrite(HOST_IA32_SYSENTER_EIP, 0);

    ret = setup_host_segment_registers();
    if (ret != 0) {
        pr_error("Error setting up host segment registers");
        return ret;
    }

    ret = setup_host_system_tables();
    if (ret != 0) {
        pr_error("Error setting up host system tables");
        return ret;
    }

    ret = setup_exit_handler(vcpu);
    if (ret != 0) {
        pr_error("Error configuring the vm exit handler");
        return ret;
    }

    return 0;
}

int
validate_vcpu_configuration(const vcpu_t *vcpu)
{
    pr_info("Validating the vcpu configuration");

    return 0;
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

    return 0;
}

