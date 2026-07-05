#include "string.h"
#include "per-cpu.h"
#include "pf_alloc.h"
#include "printf.h"
#include "asm/cpufeatures.h"
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

/**
 * TODO: completely refactor error handling.
 * 1. we can theoretically simply do a jbe
 * 2. always return a error code, and op related return values
 *  are returned via pointers passed as arguments
 */

static inline int __check_vm_op_status(const uint32_t flags);

#define check_vm_op_status() ({                             \
        uint64_t eflags;                                    \
        asm volatile ("pushfq; pop %0" : "=r"(eflags));     \
        __check_vm_op_status(eflags);                       \
        })

static inline bool
vmclear(const phys_addr_t vmcs_area_ptr)
{
    int res;

    asm volatile("vmclear %0" :: "m"(vmcs_area_ptr) : "memory");

    res = check_vm_op_status();
    if (res !=  VM_OP_STATUS_SUCCESS) {
        pr_warn("vmclear failed: %lu", res);
        return false;
    }

    return true;
}

static inline phys_addr_t
vmptrst(void)
{
    phys_addr_t current_vmcs_addr;
    asm volatile("vmptrst %0" : "+m"(current_vmcs_addr) :: "memory");
    return current_vmcs_addr;
}

static inline int
vmptrld(const phys_addr_t vmcs_ptr)
{
    asm volatile("vmptrld %0" :: "m"(vmcs_ptr) : "memory");
    return check_vm_op_status();
}

static inline uint64_t
vmread(const enum vmcs_field_encoding encoding)
{
    int res;
    /* In 64-bit mode, the destionation operand size is always 64 bit */
    uint64_t val;
    asm volatile("vmread %1, %0" : "=rm"(val) : "r"(encoding) : "memory");

    res = check_vm_op_status();
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'vmread' failed: %lu", res);
    }

    return val;
}

static inline void
vmwrite(const enum vmcs_field_encoding encoding, const uint64_t value)
{
    asm volatile("vmwrite %0, %1" :: "m"(value), "r"(encoding) : "memory");
}

static inline bool
has_current_vmcs(void)
{
    return vmptrst() != NO_CURRENT_VMCS_ADDR;
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
        return vmread(VM_INSTRUCTION_ERROR);
    }

    return VM_OP_STATUS_UNKNOWN_STATE;
}

static inline void
sanitize_cr0_for_vmx(void)
{
    const uint64_t cr0 = read_cr0();
    const uint64_t vmx_cr0_fixed0 = read_msr(MSRX64_IA32_VMX_CR0_FIXED0);
    const uint64_t vmx_cr0_fixed1 = read_msr(MSRX64_IA32_VMX_CR0_FIXED1);

    const uint64_t vmx_valid_cr0 = (cr0 | vmx_cr0_fixed0) & vmx_cr0_fixed1;
    write_cr0(vmx_valid_cr0);
}

static inline void
sanitize_cr4_for_vmx(void)
{
    const uint64_t cr4 = read_cr4();
    const uint64_t vmx_cr4_fixed0 = read_msr(MSRX64_IA32_VMX_CR4_FIXED0);
    const uint64_t vmx_cr4_fixed1 = read_msr(MSRX64_IA32_VMX_CR4_FIXED1);

    const uint64_t vmx_valid_cr4 = (cr4 | vmx_cr4_fixed0) & vmx_cr4_fixed1;
    write_cr4(vmx_valid_cr4);
}

static inline void
sanitize_control_regs_for_vmx_operation(void)
{
    sanitize_cr0_for_vmx();
    sanitize_cr4_for_vmx();
}

inline void
tag_region_with_vmx_revisionid(const phys_addr_t vmxon_region)
{
    const uint64_t msr_vmx_basic = read_msr(MSRX64_IA32_VMX_BASIC);
    const uint32_t revision_id = U64_LOWER32(msr_vmx_basic);
    virt_addr_t vaddr = phys_to_virt(vmxon_region);
    memcpy((void *) vaddr, &revision_id, sizeof(uint32_t));
}

static inline phys_addr_t
create_vmxon_region(void)
{
    virt_addr_t vaddr;
    if (get_page_zeroed(&vaddr) != 0) {
        return 0;
    }

    const phys_addr_t vmxon_region = virt_to_phys(vaddr);
    tag_region_with_vmx_revisionid(vmxon_region);
    return vmxon_region;
}

static inline phys_addr_t
create_vmcs_area(void)
{
    virt_addr_t vaddr;
    if (get_page_zeroed(&vaddr) != 0) {
        return 0;
    }

    const phys_addr_t vmcs_area_ptr = virt_to_phys(vaddr);
    tag_region_with_vmx_revisionid(vmcs_area_ptr);

    if (!vmclear(vmcs_area_ptr)) {
        free_page_raw(vmcs_area_ptr);
        pr_error("'vmclear' operation failed!");
        return 0;
    }

    return vmcs_area_ptr;
}

bool
enter_vmx_operation(void)
{
    logical_processor_t *current = get_current_logical_processor();

    sanitize_control_regs_for_vmx_operation();

    phys_addr_t vmxon_region = create_vmxon_region();
    if (!vmxon_region) {
        pr_error("Failed to create a VMXON region");
        return false;
    }

    uint64_t eflags;
    asm volatile(
            "vmxon %1   \n\t"
            "pushfq     \n\t"
            "pop %0"
            : "=r"(eflags)
            : "m"(vmxon_region)
            : "memory"
    );

    if (!__is_vm_succeed(eflags)) {
        free_page_raw(vmxon_region);
        pr_error("Failed to enter VMXON operation");
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
        return;
    }

    asm volatile("vmxoff");

    current->vmx_operation_active = false;
    free_page_raw(current->vmxon_region_ptr);
    pr_info("Left VMX operation");
}

static inline void
ensure_vcpu_current(vcpu_t *vcpu)
{
    int res;

    if (vcpu == current_vcpu) {
        return;
    }

    res = vmptrld(vcpu->arch_vcpu.vmcs_ptr);
    if (res != VM_OP_STATUS_SUCCESS) {
        pr_warn("'vmptrld' in ensure_vcpu_current() failed: %lu", res);
        return;
    }

    set_percpu_val(curr_vcpu, vcpu);
}

int
init_vcpu(vcpu_t *vcpu)
{
    const phys_addr_t vmcs_ptr = create_vmcs_area();
    if (!vmcs_ptr) {
        pr_warn("create_vmcs_area() failed");
        return -1;
    }

    vcpu->arch_vcpu.vmcs_ptr = vmcs_ptr;
    vcpu->arch_vcpu.launch_state = VMCS_LS_CLEAR;
    return 0;
}

