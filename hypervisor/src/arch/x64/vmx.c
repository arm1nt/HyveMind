#include "cpufeatures.h"
#include "types.h"
#include "vmx/vmx.h"
#include "pf_alloc.h"
#include "processor.h"
#include "printf.h"
#include "string.h"
#include "asm/paging.h"

static inline bool
enable_vmx_cr4(void)
{
    const uint64_t cr4 = read_cr4();
    write_cr4(SET_BIT(cr4, CR4_VMXE));
    return (IS_SET(read_cr4(), CR4_VMXE)) ? true : false;
}

static inline bool
confirm_in_long_mode(void)
{
    /* TODO: Sanity check */
    return true;
}

static inline void
tag_region_with_vmx_revisionid(phys_addr_t vmxon_region)
{
    const uint64_t msr_vmx_basic = read_msr(MSRX64_IA32_VMX_BASIC);
    const uint32_t revision_id = U64_LOWER32(msr_vmx_basic);
    virt_addr_t vaddr = phys_to_virt(vmxon_region);
    memcpy((void *) vaddr, &revision_id, sizeof(uint32_t));
}

bool
enter_vmx_operation(void)
{
    if (enable_vmx_cr4() == false) {
        printf("Setting VMX enable flag in CR4 failed");
        return false;
    }

    if (confirm_in_long_mode() == false) {
        printf("Processor is not in long mode");
        return false;
    }

    const uint64_t cr0 = read_cr0();
    const uint64_t vmx_cr0_fixed0 = read_msr(MSRX64_IA32_VMX_CR0_FIXED0);
    const uint64_t vmx_cr0_fixed1 = read_msr(MSRX64_IA32_VMX_CR0_FIXED1);
    const uint64_t cr4 = read_cr4();
    const uint64_t vmx_cr4_fixed0 = read_msr(MSRX64_IA32_VMX_CR4_FIXED0);
    const uint64_t vmx_cr4_fixed1 = read_msr(MSRX64_IA32_VMX_CR4_FIXED1);

    const uint64_t vmx_valid_cr0 = (cr0 | vmx_cr0_fixed0) & vmx_cr0_fixed1;
    const uint64_t vmx_valid_cr4 = (cr4 | vmx_cr4_fixed0) & vmx_cr4_fixed1;

    write_cr0(vmx_valid_cr0);
    write_cr4(vmx_valid_cr4);

    phys_addr_t vmxon_region;
    if (get_page_raw(&vmxon_region) != 0) {
        printf("Failed to allocate a VMXON region");
        return false;
    }

    tag_region_with_vmx_revisionid(vmxon_region);

    asm volatile ("vmxon %0" :: "m"(vmxon_region));

    return true;
}

void
disable_vmx_operation(void)
{
    asm volatile ("vmxoff");
}

