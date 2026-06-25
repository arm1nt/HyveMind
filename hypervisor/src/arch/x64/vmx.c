#include "asm/cpufeatures.h"
#include "types.h"
#include "vmx/vmx.h"
#include "pf_alloc.h"
#include "asm/processor.h"
#include "printf.h"
#include "string.h"
#include "asm/paging.h"

inline void
tag_region_with_vmx_revisionid(const phys_addr_t vmxon_region)
{
    const uint64_t msr_vmx_basic = read_msr(MSRX64_IA32_VMX_BASIC);
    const uint32_t revision_id = U64_LOWER32(msr_vmx_basic);
    virt_addr_t vaddr = phys_to_virt(vmxon_region);
    memcpy((void *) vaddr, &revision_id, sizeof(uint32_t));
}

static inline bool
confirm_in_long_mode(void)
{
    /* TODO: Sanity check */
    return true;
}

static inline void
validate_cr0_for_vmx(void)
{
    const uint64_t cr0 = read_cr0();
    const uint64_t vmx_cr0_fixed0 = read_msr(MSRX64_IA32_VMX_CR0_FIXED0);
    const uint64_t vmx_cr0_fixed1 = read_msr(MSRX64_IA32_VMX_CR0_FIXED1);

    const uint64_t vmx_valid_cr0 = (cr0 | vmx_cr0_fixed0) & vmx_cr0_fixed1;
    write_cr0(vmx_valid_cr0);
}

static inline void
validate_cr4_for_vmx(void)
{
    const uint64_t cr4 = read_cr4();
    const uint64_t vmx_cr4_fixed0 = read_msr(MSRX64_IA32_VMX_CR4_FIXED0);
    const uint64_t vmx_cr4_fixed1 = read_msr(MSRX64_IA32_VMX_CR4_FIXED1);

    const uint64_t vmx_valid_cr4 = (cr4 | vmx_cr4_fixed0) & vmx_cr4_fixed1;
    write_cr4(vmx_valid_cr4);
}

static inline void
validate_control_regs_for_vmx_operation(void)
{
    validate_cr0_for_vmx();
    validate_cr4_for_vmx();
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
    if (confirm_in_long_mode() == false) {
        pr_error("Processor not in long mode");
        return false;
    }

    validate_control_regs_for_vmx_operation();

    phys_addr_t vmxon_region = create_vmxon_region();
    if (!vmxon_region) {
        pr_error("Failed to create VMXON region");
        return false;
    }

    asm volatile ("vmxon %0" :: "m"(vmxon_region));

    return true;
}

void
disable_vmx_operation(void)
{
    asm volatile ("vmxoff");
}

