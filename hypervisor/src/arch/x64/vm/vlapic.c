#include "fatal.h"
#include "pf_alloc.h"
#include "string.h"
#include "timer.h"
#include "vm.h"
#include "asm/apic.h"
#include "asm/apic_defs.h"
#include "vm/vlapic.h"
#include "vmx/ept.h"
#include "vmx/vmx.h"

int
remap_vlapic_base(struct vm *vm, const gpaddr vlapic_base)
{
    int ret;
    const phys_addr_t apic_access_page = vm_apic_access_page(vm);
    struct ept_mapping_info info =
        create_ept_mapping_info(vlapic_base, PAGE_SIZE, apic_access_page);

    info.no_page_flags = EPT_RWX;
    info.page_map_flags = EPT_RWX | EPT_MAPS_PAGE | EPT_MEM_TYPE_UC_FLAG;

    if ((ret = add_ept_mapping(&vm->arch_vm.vmx.eptp, &info)) != 0) {
        return VLAPIC_ACCESS_PAGE_MAPPING_ERROR;
    }

    return VLAPIC_SUCCESS;
}

void
configure_vlapic_msr_intercepts(struct vm *vm)
{
    vmx_set_intercept_msr(vm, MSRX64_IA32_APIC_BASE, true);
    vmx_set_intercept_msr(vm, MSR_APIC_TSC_DEADLINE, true);
}

static inline void
__write_vlapic_register(const virt_addr_t base, const int reg, const uint32_t val)
{
    *((uint32_t *) (base + reg)) = val;
}

static void
reset_vlapic_state(struct vlapic *vlapic)
{
    const virt_addr_t page = phys_to_virt(vlapic->virtual_apic_page);
    memset((void*) page, 0, PAGE_SIZE);

    const uint32_t formatted_apic_id_reg = vlapic->physical_apic_id<< XAPIC_ID_SHIFT;
    __write_vlapic_register(page, APIC_ID_REG, formatted_apic_id_reg);
    __write_vlapic_register(page, XAPIC_APR_REG, formatted_apic_id_reg);
    __write_vlapic_register(page, APIC_VERSION_REG, get_apic_version());
    __write_vlapic_register(page, XAPIC_DEST_FORMAT_REG, ~U32(0));

    __write_vlapic_register(page, APIC_LVT_CMCI_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_TIMER_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_THERMAL_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_PERFMON_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_LINT0_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_LINT1_REG, APIC_LVT_RESET_VAL);
    __write_vlapic_register(page, APIC_LVT_ERROR_REG, APIC_LVT_RESET_VAL);

    __write_vlapic_register(page, APIC_SVR_REG, APIC_SVR_RESET_VAL);
}

int
init_vlapic(vcpu_t *vcpu, const lapicid_t initial_apic_id, const bool is_bsp)
{
    phys_addr_t virtual_apic_page;
    struct vlapic *vlapic = vcpu_vlapic(vcpu);

    if (get_page_raw(&virtual_apic_page) != 0)
        return -ERR_NO_MEM;

    vlapic->physical_apic_id = initial_apic_id;
    vlapic->virtual_apic_page = virtual_apic_page;
    reset_vlapic_state(vlapic);

    apic_base_msr_t apic_base;
    apic_base.raw = 0;
    apic_base.bsp = is_bsp;
    apic_base.extd = 0;
    apic_base.global_enable = 1;
    apic_base.base = APIC_MEM_BASE;

    vlapic->apic_base_msr = apic_base;
    vlapic->mode = VLAPIC_XAPIC;

    return VLAPIC_SUCCESS;
}

