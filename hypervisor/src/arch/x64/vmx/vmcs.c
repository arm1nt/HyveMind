#include "printf.h"
#include "pf_alloc.h"
#include "vm.h"
#include "vmx/vmcs.h"

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

void
dump_vmcs(const vcpu_t *vcpu)
{
    pr_info("todo: dump vmcs");
}

