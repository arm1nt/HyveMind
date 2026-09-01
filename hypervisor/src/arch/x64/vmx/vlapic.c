#include "fatal.h"
#include "pf_alloc.h"
#include "string.h"
#include "vm.h"
#include "asm/apic.h"
#include "asm/apic_defs.h"
#include "vmx/ept.h"
#include "vmx/vlapic.h"

int
guest_wrmsr_apic_base_msr(vcpu_t *vcpu, const uint64_t val)
{
    NOT_YET_IMPLEMENTED;
}

int
remap_vlapic_base(struct vm *vm, const gpaddr vlapic_base)
{
    NOT_YET_IMPLEMENTED;
}

static inline void
__write_vlapic_register(const virt_addr_t base, const int reg, const uint32_t val)
{
    *((uint32_t *) base + reg) = val;
}

static inline void
write_vlapic_register(struct vlapic *vlapic, const int reg, const uint32_t val)
{
    __write_vlapic_register(phys_to_virt(vlapic->virtual_apic_page), reg, val);
}

uint64_t
get_vlapic_mem_base(const struct vlapic *vlapic)
{
    return U64_LSHIFT(vlapic->apic_base_msr.base, PAGE_SHIFT);
}

static void
reset_vlapic_state(struct vlapic *vlapic)
{
    const virt_addr_t page = phys_to_virt(vlapic->virtual_apic_page);
    memset((void*) page, 0, PAGE_SIZE);

    __write_vlapic_register(page, APIC_ID_REG, vlapic->initial_apic_id);
    const uint32_t formatted_apic_id_reg = vlapic->initial_apic_id << XAPIC_ID_SHIFT;
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
init_vlapic(vcpu_t *vcpu, const uint32_t initial_apic_id)
{
    struct vlapic *vlapic = vcpu_vlapic(vcpu);

    vlapic->initial_apic_id = initial_apic_id;

    if (get_page_raw(&vlapic->virtual_apic_page) != 0) {
        pr_error("Failed to allocate virtual apic page");
        return ERR_NO_MEM;
    }

    reset_vlapic_state(vlapic);

    apic_base_msr_t apic_base;
    apic_base.raw = 0;
    apic_base.bsp = vcpu->arch.is_bsp;
    apic_base.extd = 0;
    apic_base.global_enable = 1;
    apic_base.base = APIC_MEM_BASE >> 12;
    vlapic->apic_base_msr.raw = apic_base.raw;

    vlapic->mode = VLAPIC_XAPIC;

    return VLAPIC_SUCCESS;
}

