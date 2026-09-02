#include "fatal.h"
#include "halloc.h"
#include "pf_alloc.h"
#include "printf.h"
#include "vm.h"
#include "asm/apic.h"
#include "vmx/ept.h"
#include "vmx/policy.h"
#include "vmx/vmx.h"
#include "loader/loader.h"

extern void vm_entry_test(void);

int
copy_to_vm_gpaddr(struct vm *vm, const gpaddr start, const void *data, const uint64_t size)
{
    if (!vm_memory_contig_range_fits(vm, start, size)) {
        pr_error("Cannot copy to the VM as the content to be copied doesn't fit "
                "into the VM's allocated memory!"
        );
        return -1;
    }

    struct vm_physical_memory vm_mem = vm->phys_mem;
    phys_addr_t host_pa_start = vm_mem.start + start;

    memcpy((void *) phys_to_virt(host_pa_start), data, size);
    return 0;
}

inline uint64_t
get_vm_memory_size(const struct vm *vm)
{
    return (vm->phys_mem.end - vm->phys_mem.start) + 1;
}

inline bool
vm_memory_contig_range_fits(
        const struct vm *vm,
        const gpaddr start,
        const uint64_t size
) {
    const uint64_t total_size = get_vm_memory_size(vm);
    const gpaddr highest_addr = total_size - 1;

    return start <= highest_addr && size <= total_size - start;
}

static int
virtualize_guest_physical_memory(struct vm *vm, const struct guest_config *config)
{
    phys_addr_t contig_vm_mem_start;
    uint64_t req_bytes, req_pages;
    struct ept_mapping_info info;
    eptp_t eptp;

    req_bytes = get_req_mem_size_bytes(config);
    req_pages = bytes_to_nr_pages(req_bytes);

    if (get_pages_raw(req_pages, &contig_vm_mem_start) != 0) {
        pr_error("Failed to allocate host phys memory for VM");
        return -1;
    }

    info = create_ept_mapping_info(0, req_bytes, contig_vm_mem_start);
    info.no_page_flags = EPT_RWX;
    info.page_map_flags = EPT_MAPS_PAGE | EPT_RWX | EPT_MEM_TYPE_WB_FLAG;

    try_use_mb_mappings(&info);
    try_use_gb_mappings(&info);

    if (create_ept_mapping(&eptp, &info) != EPT_SUCCESS) {
        pr_error("Failed to create EPT mapping");
        hfree((void *) contig_vm_mem_start);
        destroy_ept_mapping(&eptp);
        return -1;
    }

    vm->phys_mem.start = contig_vm_mem_start;
    vm->phys_mem.end = contig_vm_mem_start + (req_pages * PAGE_SIZE) - 1;
    vm->arch_vm.vmx.eptp = eptp;
    return 0;
}

static inline void
init_default_virt_policy(struct vmx_virt_policy *policy)
{
    vmx_init_default_policy(policy);
}

static int
set_guest_info(vcpu_t *vcpu, struct vcpu_guest_reg_state *state)
{
    int ret = 0;

    vcpu->arch.state.user_regs = state->uregs;

    ret |= vmx_set_guest_cr0(vcpu, state->cr0);
    vmx_set_guest_cr3(vcpu, state->cr3);
    vmx_set_guest_cr4(vcpu, state->cr4);

    vmx_set_guest_efer(vcpu, state->efer);

    ret |= vmx_set_segment_register(vcpu, X86_CS_REG, state->segments.cs);
    ret |= vmx_set_segment_register(vcpu, X86_SS_REG, state->segments.ss);
    ret |= vmx_set_segment_register(vcpu, X86_DS_REG, state->segments.ds);
    ret |= vmx_set_segment_register(vcpu, X86_ES_REG, state->segments.es);
    ret |= vmx_set_segment_register(vcpu, X86_FS_REG, state->segments.fs);
    ret |= vmx_set_segment_register(vcpu, X86_GS_REG, state->segments.gs);
    ret |= vmx_set_segment_register(vcpu, X86_TR_REG, state->segments.tr);
    ret |= vmx_set_segment_register(vcpu, X86_LDTR_REG, state->segments.ldtr);

    ret |= vmx_set_system_table(vcpu, X86_GDT, state->gdtr);
    ret |= vmx_set_system_table(vcpu, X86_IDT, state->idtr);

    return ret;
}

static int
init_vm_mirroring_vmm(struct vm *vm, const struct guest_config *config)
{
    int ret;
    vcpu_t *bsp;
    virt_addr_t stack_bot;
    struct vmx_virt_policy *policy;
    struct vcpu_guest_reg_state vcpu_guest_state;

    bsp = vm->vcpus[0];
    policy = bsp->arch.hw.vmx.virt_policy;

    init_default_virt_policy(policy);
    policy->entry_ctls.ia32e_mode_guest = 1;
    policy->exit_ctls1.host_addr_space_size = 1;

    if ((ret = validate_vmx_virt_policy(policy)) != VMX_POLICY_VALID) {
        pr_error("Configured virt policy cannot be realized: %lu", U64(ret));
        return -1;
    }

    if (vmx_initialize_vmcs_area(bsp) != 0) {
        pr_error("Failed to initialize vcpu's VMCS area");
        return ret;
    }

    if (get_pages_zeroed(DEFAULT_VCPU_STACK_PAGES, &stack_bot) != 0) {
        pr_error("Failed to allocate pages for guest stack");
        return -1;
    }

    init_guest_reg_state_mirroring_host(&vcpu_guest_state);
    vcpu_guest_state.uregs.rsp = (stack_bot + (DEFAULT_VCPU_STACK_PAGES * PAGE_SIZE)) - 8;
    vcpu_guest_state.uregs.rip = __vaddr(vm_entry_test);

    if (set_guest_info(bsp, &vcpu_guest_state) != 0) {
        pr_error("Failed to initialize guest register state");
        free_pages(DEFAULT_VCPU_STACK_PAGES, stack_bot);
        return -1;
    }

    /* remove later */
    vmx_enter_vcpu(bsp);
    die_reason("blocker");
}

static inline void
configure_linux_32bit_policy(struct vmx_virt_policy *policy)
{
    init_default_virt_policy(policy);

    policy->proc_ctls1.activate_secondary_controls = 1;
    policy->proc_ctls1.msr_bitmaps = 1;
    policy->proc_ctls2.enable_invpcid = 1;
    policy->proc_ctls2.enable_ept = 1;
    policy->proc_ctls2.unrestricted_guest = 1;
    policy->proc_ctls2.enable_rdtscp = 1;

    policy->exit_ctls1.host_addr_space_size = 1;
    policy->exit_ctls1.load_ia32_efer = 1;
    policy->exit_ctls1.save_ia32_efer = 1;

    policy->entry_ctls.load_ia32_efer = 1;
}

static inline int
add_permissive_msr_bitmap(struct vm *vm)
{
    virt_addr_t msr_bitmap;

    if (get_page_zeroed(&msr_bitmap) != 0) {
        pr_error("Failed to allocate msr bitmap");
        return -1;
    }

    vm->arch_vm.vmx.msr_bitmap_addr = virt_to_phys(msr_bitmap);
    return 0;
}

static int
init_vm_linux_direct_boot_32bit(struct vm *vm, const struct guest_config *config)
{
    int ret;
    vcpu_t *bsp;
    struct vmx_virt_policy *policy;
    struct vcpu_guest_reg_state guest_state;
    struct linux_load_info load_info;
    cr4_t cr4_mask, cr4_shadow;

    bsp = vm->vcpus[0];
    bsp->arch.is_bsp = true;
    policy = bsp->arch.hw.vmx.virt_policy;

    configure_linux_32bit_policy(policy);
    if ((ret = validate_vmx_virt_policy(policy)) != VMX_POLICY_VALID) {
        pr_error("Configured virt policy is invalid: %lu", U64(ret));
        return -1;
    }

    if ((ret = init_vlapic(bsp, 0)) != VLAPIC_SUCCESS) {
        pr_error("Failed to initialize the bsp's vlapic: %lu", ret);
        return -1;
    }

    vmx_get_default_cr4_mask_and_shadow(&cr4_mask.raw, &cr4_shadow.raw);
    /* Hide VMX enablement */
    cr4_shadow.vmxe = 0;
    vmx_set_cr4_mask_and_shadow(bsp, cr4_mask.raw, cr4_shadow.raw);

    if (add_permissive_msr_bitmap(vm) != 0) {
        pr_error("Failed to allocate permissive msr bitmap");
        return -1;
    }

    if (virtualize_guest_physical_memory(vm, config) != 0) {
        pr_error("Failed to create virtualized memory area for the VM");
        return -1;
    }

    const gpaddr vlapic_mem_base = get_vlapic_mem_base(vcpu_vlapic(bsp));
    if ((ret = remap_vlapic_base(vm, vlapic_mem_base)) != VLAPIC_SUCCESS) {
        return -1;
    }

    if (vmx_initialize_vmcs_area(bsp) != 0) {
        pr_error("Failed to initialize linux guest bsp's VMCS area");
        return -1;
    }

    if (load_linux_32bit_direct_boot_for_vm(vm, config, &load_info) != 0) {
        pr_error("Failed to load & setup kernel for the guest");
        return -1;
    }

    init_guest_reg_state_for_linux_32bit(&guest_state);
    guest_state.gdtr.base = load_info.gdt_page;
    guest_state.uregs.eip = load_info.pm_kernel_entry_point;
    guest_state.uregs.rsi = load_info.zero_page;

    if (set_guest_info(bsp, &guest_state) != 0) {
        pr_error("Failed to initialize guest register state");
        return -1;
    }

    /* todo: remove later */
    vmx_enter_vcpu(bsp);

    die_reason("blocker");
}

int
arch_init_vm(struct vm *vm, const struct guest_config *config)
{
    vm->arch_vm.vmx.ops = emulate_ops;

    switch (config->guest_type) {
        case LINUX_DIRECT_BOOT_32BIT:
            return init_vm_linux_direct_boot_32bit(vm, config);
        case MIRROR_VMM:
            return init_vm_mirroring_vmm(vm, config);
        default:
            pr_error("Unsupported VM guest type configured");
            return -1;
    }

    die_reason("Unreachable");
}

void
destroy_arch_vm(struct vm *vm)
{
    /* De-allocate eptp structures, bitmaps, etc. */
    NOT_YET_IMPLEMENTED;
}

int
allocate_arch_vcpu(struct vcpu *vcpu)
{
    int ret;

    vcpu->arch.hw.vmx.virt_policy = hmalloc(sizeof(struct vmx_virt_policy));
    if (!vcpu->arch.hw.vmx.virt_policy) {
        pr_error("Failed to allocate a vmx virt policy struct!");
        return -1;
    }

    ret = vmx_vcpu_allocate(vcpu);
    if (ret != 0) {
        pr_debug("Failed to allocate the vmcs area for the vcpu");
        hfree(vcpu->arch.hw.vmx.virt_policy);
        return ret;
    }

    return 0;
}

void
destroy_arch_vcpu(struct vcpu *vcpu)
{
    if (!vcpu) {
        return;
    }

    pr_debug("Destroying arch vcpu members");

    vmx_destroy_vcpu(vcpu);

    if (vcpu->arch.hw.vmx.virt_policy) {
        hfree(vcpu->arch.hw.vmx.virt_policy);
    }
}

