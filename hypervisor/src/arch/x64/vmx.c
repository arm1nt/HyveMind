#include "string.h"
#include "pf_alloc.h"
#include "printf.h"
#include "asm/cpufeatures.h"
#include "asm/vmm.h"
#include "asm/paging.h"
#include "asm/processor.h"
#include "vmx/vmx.h"

#define NO_CURRENT_VMCS_ADDR 0xFFFFFFFFFFFFFFFF

static inline void
vmxon(const phys_addr_t vmxon_region)
{
    asm volatile("vmxon %0" :: "m"(vmxon_region) : "memory");
}

static inline phys_addr_t
vmptrst(void)
{
    phys_addr_t current_vmcs_addr;
    asm volatile("vmptrst %0" : "+m"(current_vmcs_addr) :: "memory");
    return current_vmcs_addr;
}

static inline bool
has_current_vmcs(void)
{
    return vmptrst() != NO_CURRENT_VMCS_ADDR;
}

static inline bool
is_vm_succeed(void)
{
    const uint32_t eflags = read_rflags();
    return IS_SET(
            eflags,
            EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_OF
    );
}

static inline bool
is_vm_fail(void)
{
    return false;
}

static inline bool
is_vm_fail_invalid(void)
{
    const uint32_t eflags = read_rflags();
    const bool set_res = IS_SET(eflags, EFLAGS_CF);
    const bool clear_res = IS_CLEAR(
            eflags,
            EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_OF
    );
    return set_res && clear_res;
}

static inline bool
is_vm_fail_valid(void)
{
    return false;
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

    vmxon(vmxon_region);

    if (is_vm_fail_invalid()) {
        pr_error("invalid vmxon region memory area");
        return false;
    }

    if (is_vm_succeed()) {
        current->vmxon_region_ptr = vmxon_region;
        current->vmx_operation_active = true;
        pr_info("Successfully entered VMX operation");
        return true;
    } else {
        pr_error("Failed to enter VMX operation");
        free_page_raw(vmxon_region);
        current->vmx_operation_active = false;
        return false;
    }
}

void
leave_vmx_operation(void)
{
    logical_processor_t *current = get_current_logical_processor();


    current->vmx_operation_active = false;
    /* free the allocated VMXON area */
}

