#include "fatal.h"
#include "halloc.h"
#include "pf_alloc.h"
#include "vm.h"
#include "asm/arch_types.h"
#include "asm/apic_defs.h"
#include "loader/loader.h"
#include "vmx/ept.h"
#include "vmx/policy.h"
#include "vmx/vmx.h"

int
arch_alloc_vcpu(struct vcpu *vcpu)
{
    spinlock_init(vcpu_event_lock(vcpu));
    return vmx_vcpu_allocate(vcpu);
}

void
arch_destroy_vcpu(struct vcpu *vcpu)
{
    if (!vcpu)
        return;

    vmx_destroy_vcpu(vcpu);

    if (vcpu_virt_policy(vcpu))
        hfree(vcpu_virt_policy(vcpu));
}

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

static void
destroy_virtualized_guest_mem(struct vm *vm)
{
    const uint64_t size_bytes = get_vm_memory_size(vm);
    const uint64_t pages = bytes_to_nr_pages(size_bytes);

    free_pages_raw(pages, vm->phys_mem.start);
    destroy_ept_mapping(&vm_vmx_state(vm).eptp);
}

static int
virtualize_guest_physical_mem(struct vm *vm, const struct vm_config *config)
{
    phys_addr_t contig_vm_mem_start;
    uint64_t req_bytes, req_pages;
    struct ept_mapping_info info;
    eptp_t eptp;

    req_bytes = get_vm_config_req_bytes(config);
    req_pages = bytes_to_nr_pages(req_bytes);

    if (get_pages_raw(req_pages, &contig_vm_mem_start) != 0) {
        pr_error("Failed to allocate host phys memory for VM");
        return -ERR_NO_MEM;
    }

    info = create_ept_mapping_info(0, req_bytes, contig_vm_mem_start);
    info.no_page_flags = EPT_RWX;
    info.page_map_flags = EPT_MAPS_PAGE | EPT_RWX | EPT_MEM_TYPE_WB_FLAG;

    try_use_mb_mappings(&info);
    try_use_gb_mappings(&info);

    if (create_ept_mapping(&eptp, &info) != EPT_SUCCESS) {
        pr_error("Failed to create EPT mapping for VM %s", vm->name);
        free_pages_raw(req_pages, contig_vm_mem_start);
        destroy_ept_mapping(&eptp);
        return -1;
    }

    vm->phys_mem.start = contig_vm_mem_start;
    vm->phys_mem.end = contig_vm_mem_start + (req_pages * PAGE_SIZE) - 1;
    vm->arch_vm.vmx.eptp = eptp;
    return 0;
}

/**
 * Allocate devices exposed to the VM and setup IRQ & port mappings.
 *  -> For now, the mappings are hardcoded as we support a fixed set of devices
 *     (i.e. PIC, UART, PIT) for every VM.
 */
static int
prepare_vm_env_devices(struct vm *vm, const struct vm_config *config)
{
    /* Per-default, we don't expose any host IRQ lines to the VM */
    for (int i = 0; i < 256; i++) {
        vm->arch_vm.devices.host_to_guest_irq[i] = -1;
        vm->arch_vm.devices.guest_to_host_irq[i] = -1;
    }

    /* Every IO port access should cause a VM-exit */
    virt_addr_t io_bitmap;
    if (get_pages_zeroed(VMCS_IO_BITMAP_PAGES, &io_bitmap) != 0) {
        pr_error("Error allocating IO bitmaps for VM %s", config->name);
        return -ERR_NO_MEM;
    }
    memset((uint8_t *) io_bitmap, 0xFF, PAGE_SIZE * VMCS_IO_BITMAP_PAGES);
    vm_io_bitmap_addr(vm) = virt_to_phys(io_bitmap);

    vm_vpic(vm) = allocate_vpic();
    if (!vm_vpic(vm)) {
        pr_error("Error allocating a vPIC for VM %s", vm->name);
        free_pages(VMCS_IO_BITMAP_PAGES, io_bitmap);
        return -1;
    }

    const struct guest_uart_info vm_uart = config->devices.uart_info;
    if (vm_uart.configured) {
        vm->arch_vm.devices.host_serial_pio_base = vm_uart.host_base_port;
        vm->arch_vm.devices.host_to_guest_irq[vm_uart.host_irq] = vm_uart.vm_irq;
        vm->arch_vm.devices.guest_to_host_irq[vm_uart.vm_irq] = vm_uart.host_irq;
    }

    return 0;
}

/**
 * Must be called after an EPT mapping was created since we need to map the
 * APIC access page into the VM's addr space
 */
static int
prepare_apic_virtualization(struct vm *vm, const struct vm_config *config)
{
    lapicid_t lapic_id_counter = 0;
    phys_addr_t apic_access_page;

    if (get_page_raw(&apic_access_page) != 0)
        return -ERR_NO_MEM;

    vm_apic_access_page(vm) = apic_access_page;

    if (remap_vlapic_base(vm, APIC_MEM_BASE) != VLAPIC_SUCCESS) {
        pr_error("Error mapping apic-access-page to addr 0x%lx in VM %s's addr space",
                APIC_MEM_BASE,
                vm->name
        );
        free_page_raw(apic_access_page);
        return -1;
    }

    configure_vlapic_msr_intercepts(vm);

    for (int i = 0; i < vm->nr_vcpus; i++) {
        int ret = init_vlapic(vm->vcpus[i], lapic_id_counter, lapic_id_counter == 0);
        if (ret != 0) {
            pr_error("Failed to initialize vlapic for vCPU (%lu) of VM %s", i, vm->name);
            NOT_YET_IMPLEMENTED;
        }
        lapic_id_counter++;
    }

    return 0;
}

static int
prepare_general_vm_environment(struct vm *vm, const struct vm_config *config)
{
    virt_addr_t msr_bitmap;

    if (get_page_zeroed(&msr_bitmap) != 0) {
        pr_error("Failed to allocate MSR bitmap for VM %s", vm->name);
        return -ERR_NO_MEM;
    }

    vm_msr_bitmap_addr(vm) = virt_to_phys(msr_bitmap);
    return 0;
}

/* TODO: move into vcpu.c */
static int
set_vcpu_reg_state(vcpu_t *vcpu, struct vcpu_guest_reg_state *state)
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
prepare_env_linux_boot_32bit(struct vm *vm, const struct vm_config *config)
{
    struct linux_load_info load_info;
    struct vcpu_guest_reg_state vcpu_reg_state;

    if (load_linux_32bit_direct_boot_for_vm(vm, config, &load_info) != 0) {
        pr_error("Failed to load Linux kernel for VM %s", vm->name);
        return -1;
    }

    vcpu_t *bsp = vm->vcpus[0];
    init_guest_reg_state_for_linux_32bit(&vcpu_reg_state);
    vcpu_reg_state.gdtr.base = load_info.gdt_page;
    vcpu_reg_state.uregs.eip = load_info.pm_kernel_entry_point;
    vcpu_reg_state.uregs.rsi = load_info.zero_page;
    if (set_vcpu_reg_state(bsp, &vcpu_reg_state) != 0) {
        pr_error("Failed to set reg state of the BSP vCPU of VM %s", vm->name);
        return -1;
    }

    for (int i = 1; i < vm->nr_vcpus; i++) {
        vcpu_t *ap = vm->vcpus[i];
        reset_guest_reg_state_to_init_state(&vcpu_reg_state);
        if (set_vcpu_reg_state(ap, &vcpu_reg_state) != 0) {
            pr_error("Failed to set reg state of AP vCPU of VM %s", vm->name);
            return -1;
        }
    }

    return 0;
}

static inline int
prepare_env_for_guest(struct vm *vm, const struct vm_config *config)
{
    switch (config->type) {
        case LINUX_DIRECT_BOOT_32BIT:
            /* Todo: eventually remove the ops struct */
            vm->arch_vm.vmx.ops = emulate_ops;
            return prepare_env_linux_boot_32bit(vm, config);
        default:
            die_reason("Unsupported guest type");
    }
}

static inline void
configure_base_virt_policy(struct vmx_virt_policy *policy)
{
    vmx_init_default_policy(policy);

    policy->pin_ctls.external_interrupt_exiting = 1;

    policy->proc_ctls1.activate_secondary_controls = 1;
    policy->proc_ctls1.msr_bitmaps = 1;
    policy->proc_ctls1.io_bitmaps = 1;
    policy->proc_ctls1.tpr_shadow = 1;

    policy->proc_ctls2.enable_invpcid = 1;
    policy->proc_ctls2.enable_ept = 1;
    policy->proc_ctls2.unrestricted_guest = 1;
    policy->proc_ctls2.enable_rdtscp = 1;
    policy->proc_ctls2.virtualize_apic_accesses = 1;
    policy->proc_ctls2.apic_register_virtualization = 1;

    policy->exit_ctls1.host_addr_space_size = 1;
    policy->exit_ctls1.load_ia32_efer = 1;
    policy->exit_ctls1.save_ia32_efer = 1;
    policy->exit_ctls1.ack_interrupt_on_exit = 1;

    policy->entry_ctls.load_ia32_efer = 1;
}

int
arch_prepare_execution_env(struct vm *vm, const struct vm_config *config)
{
    if (prepare_general_vm_environment(vm, config) != 0)
        return -1;

    if (virtualize_guest_physical_mem(vm, config) != 0) {
        pr_error("Failed to virtualize memory for VM %s", vm->name);
        free_page_raw(vm_msr_bitmap_addr(vm));
        return -1;
    }

    if (prepare_vm_env_devices(vm, config) != 0) {
        pr_error("Failed to configure the devices exposed to VM %s", vm->name);
        destroy_virtualized_guest_mem(vm);
        return -1;
    }

    if (prepare_apic_virtualization(vm, config) != 0) {
        pr_error("Failed to prepare APIC virtualization for VM %s", vm->name);
        destroy_virtualized_guest_mem(vm);
        /* Todo: free everything that was allocated for the vm_env_devices */
        NOT_YET_IMPLEMENTED;
    }

    for(int i = 0; i < vm->nr_vcpus; i++) {
        vcpu_t *vcpu = vm->vcpus[i];

        vcpu_virt_policy(vcpu) = (struct vmx_virt_policy *) hmalloc(sizeof(struct vmx_virt_policy));
        if (!vcpu_virt_policy(vcpu)) {
            NOT_YET_IMPLEMENTED;
        }

        configure_base_virt_policy(vcpu_virt_policy(vcpu));

        if (vmx_initialize_vmcs_area(vcpu) != 0) {
            pr_error("Initializing VMCS area of vCPU (nr %lu) in VM %s failed", i, vm->name);
            NOT_YET_IMPLEMENTED;
        }
    }

    if (prepare_env_for_guest(vm, config) != 0) {
        NOT_YET_IMPLEMENTED;
    }

    /**
     * At this point, the execution environment of the VM is completely set up
     * and all vCPUs (and their VMCS) are properly configured and are schedulable.
     * -> Todo:
     *      Vmclear each vCPU and set their status to schedulable
     */

    NOT_YET_IMPLEMENTED;
}

