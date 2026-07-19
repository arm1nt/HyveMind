#include "fatal.h"
#include "halloc.h"
#include "pf_alloc.h"
#include "types.h"
#include "vm.h"
#include "printf.h"
#include "asm/gdt_idt.h"
#include "asm/mm.h"
#include "asm/vm.h"
#include "asm/paging.h"
#include "asm/processor.h"
#include "asm/pgtables.h"
#include "asm/segmentation.h"
#include "vmx/vmx.h"

static inline void
sanitize_cr0_register_for_vmx_operation(union cr0 *cr0)
{
    const uint64_t old_cr0_raw = cr0->raw;
    const uint64_t vmx_cr0_fixed0 = read_msr(MSRX64_IA32_VMX_CR0_FIXED0);
    const uint64_t vmx_cr0_fixed1 = read_msr(MSRX64_IA32_VMX_CR0_FIXED1);
    cr0->raw = (cr0->raw | vmx_cr0_fixed0) & vmx_cr0_fixed1;

    if (old_cr0_raw != cr0->raw) {
        pr_info("CR0 sanitization changed cr0. Old (%lx) vs. new (%lx)",
                old_cr0_raw,
                cr0->raw
        );
    }
}

static inline void
sanitize_cr4_register_for_vmx_operation(union cr4 *cr4)
{
    const uint64_t old_cr4_raw = cr4->raw;
    const uint64_t vmx_cr4_fixed0 = read_msr(MSRX64_IA32_VMX_CR4_FIXED0);
    const uint64_t vmx_cr4_fixed1 = read_msr(MSRX64_IA32_VMX_CR4_FIXED1);
    cr4->raw = (cr4->raw | vmx_cr4_fixed0) & vmx_cr4_fixed1;

    if (old_cr4_raw != cr4->raw) {
        pr_info("CR4 sanitization changed cr4: old (%lx) vs. new (%lx)",
                old_cr4_raw,
                cr4->raw
        );
    }
}

/******************************************************************************
 * Guest state area setup
******************************************************************************/

/* todo: */

/******************************************************************************
 * Host state area setup
******************************************************************************/

extern void asm_vmx_exit_handler(void);

void
vmx_exit_handler(void)
{
    pr_info("vmx exit handler called");
}

static inline void
set_host_cr0_register(vcpu_t *vcpu)
{
    union cr0 host_cr0;
    host_cr0.raw = read_cr0();

    /**
     * The guest should also run in 64-bit mode. So at least CR0.PG and
     * CR0.PE must be set.
     */
    host_cr0.pg = 1;
    host_cr0.wp = 1;
    host_cr0.pe = 1;

    sanitize_cr0_register_for_vmx_operation(&host_cr0);
    vmx_write_quadword(vcpu, HOST_CR0, host_cr0.raw);
}

static inline void
set_host_cr3_register(vcpu_t *vcpu)
{
    uint64_t raw_cr3 = read_cr3();
    struct cr3 *host_cr3 = (struct cr3 *) &raw_cr3;

    host_cr3->ignored0 = 0;
    host_cr3->ignored1 = 0;
    host_cr3->reserved0 = 0;
    host_cr3->reserved1 = 0;

    host_cr3->paddr = host_cr3->paddr & percpu_val(max_phys_addr);

    vmx_write_quadword(vcpu, HOST_CR3, raw_cr3);
}

static inline void
set_host_cr4_register(vcpu_t *vcpu)
{
    union cr4 host_cr4;
    host_cr4.raw = read_cr4();

    /* Must be 1 when 'host_address_space_size' in VM-exit control is 1 */
    host_cr4.pae = 1;

    sanitize_cr4_register_for_vmx_operation(&host_cr4);
    vmx_write_quadword(vcpu, HOST_CR4, host_cr4.raw);
}

static inline void
setup_host_control_registers(vcpu_t *vcpu)
{
    set_host_cr0_register(vcpu);
    set_host_cr3_register(vcpu);
    set_host_cr4_register(vcpu);
}

static void
setup_host_cs_selector(vcpu_t *vcpu)
{
    const segment_selector_t cs = DEFINE_SEGMENT_SELECTOR(HYVEMIND_CS_SEGMENT_INDEX, TI_GDT, 0);
    const uint16_t raw_cs = *((uint16_t *) &cs);

    if (raw_cs == 0) {
        die_reason("CS selector field is 0x00");
    }

    vmx_write_quadword(vcpu, HOST_CS_SELECTOR, (uint64_t) raw_cs);
}

static void
setup_host_tr_selector(vcpu_t *vcpu)
{
    const segment_selector_t tr = read_task_register();
    const uint16_t raw_tr = *((uint16_t *) &tr);

    if (raw_tr == 0) {
        die_reason("Host TR selector is 0x00");
    }

    vmx_write_quadword(vcpu, HOST_TR_SELECTOR, (uint64_t) raw_tr);
}

static inline void
setup_host_selector_fields(vcpu_t *vcpu)
{
    setup_host_cs_selector(vcpu);
    setup_host_tr_selector(vcpu);

    vmx_write_quadword(vcpu, HOST_SS_SELECTOR, 0);
    vmx_write_quadword(vcpu, HOST_DS_SELECTOR, 0);
    vmx_write_quadword(vcpu, HOST_ES_SELECTOR, 0);
    vmx_write_quadword(vcpu, HOST_FS_SELECTOR, 0);
    vmx_write_quadword(vcpu, HOST_GS_SELECTOR, 0);
}

static inline void
setup_host_base_address_fields(vcpu_t *vcpu)
{
    const gdt_ptr_t gdtr = read_gdtr();
    const idt_ptr_t idtr = read_idtr();
    const virt_addr_t tr_base = get_current_tss_base();

    if (!is_paging_canonical(gdtr.base)) {
        pr_error("GDTR base address is not canonical: %lx", gdtr.base);
        die();
    }

    if (!is_paging_canonical(idtr.base)) {
        pr_error("IDTR base address is not canonical: %lx", idtr.base);
        die();
    }

    if (!is_paging_canonical(tr_base)) {
        pr_error("TSS base address is not canonical: %lx", tr_base);
        die();
    }

    vmx_write_quadword(vcpu, HOST_GDTR_BASE, gdtr.base);
    vmx_write_quadword(vcpu, HOST_IDTR_BASE, idtr.base);
    vmx_write_quadword(vcpu, HOST_TR_BASE, tr_base);

    vmx_write_quadword(vcpu, HOST_FS_BASE, 0);
    vmx_write_quadword(vcpu, HOST_GS_BASE, 0);
}

static void
setup_exit_handler_and_stack(vcpu_t *vcpu)
{
    const int nr_handler_stack_pages = 10;
    virt_addr_t stack_bot, exit_handler_rsp_val;
    const virt_addr_t exit_handler_addr = (virt_addr_t) asm_vmx_exit_handler;

    if (!is_paging_canonical(exit_handler_addr)) {
        pr_error("Exit handler function address is not canonical: %lx", exit_handler_addr);
        die();
    }

    vmx_write_quadword(vcpu, HOST_RIP, exit_handler_addr);

    if (get_pages_zeroed(nr_handler_stack_pages, &stack_bot) != 0) {
        die_reason("Failed to allocate pages for the vm-exit handler function stack");
    }

    if (!is_paging_canonical(stack_bot)) {
        pr_error("Allocated 'stack_bot' addr is not canonical : %lx", stack_bot);
        die();
    }

    exit_handler_rsp_val = (stack_bot + (nr_handler_stack_pages * PAGE_SIZE)) - 8;
    if (!is_paging_canonical(exit_handler_rsp_val)) {
        pr_error("exit_handler_rsp address is not canonical: %lx", exit_handler_rsp_val);
        die();
    }

    vmx_write_quadword(vcpu, HOST_RSP, exit_handler_rsp_val);
}

static inline void
setup_host_state_area(vcpu_t *vcpu)
{
    setup_host_control_registers(vcpu);

    /* IA32_SYSENTER_ESP/EIP must contain a canonical address, so we set it to 0 */
    vmx_write_quadword(vcpu, HOST_IA32_SYSENTER_ESP, 0);
    vmx_write_quadword(vcpu, HOST_IA32_SYSENTER_EIP, 0);

    setup_host_selector_fields(vcpu);
    setup_host_base_address_fields(vcpu);

    setup_exit_handler_and_stack(vcpu);
}

/******************************************************************************
 * VM execution control field setup
******************************************************************************/

#define VMX_DEFAULT1_NULLABLE()                 \
    (IS_SET(read_msr(MSRX64_IA32_VMX_BASIC), VMX_BASIC_DEFAULT1_MAY_BE_0))

#define PINBASED_CTLS_DEFAULT1_MASK             \
    ((U32_LSHIFT(1, 1)) | (U32_LSHIFT(1, 2)) | (U32_LSHIFT(1, 4)))

#define PRIMARY_PROCBASED_CTLS_DEFAULT1_MASK    \
    (                                           \
        U32_LSHIFT(1, 1)                        \
        | U32_LSHIFT(1, 4)                      \
        | U32_LSHIFT(1, 5)                      \
        | U32_LSHIFT(1, 6)                      \
        | U32_LSHIFT(1, 8)                      \
        | U32_LSHIFT(1, 13)                     \
        | U32_LSHIFT(1, 14)                     \
        | U32_LSHIFT(1, 15)                     \
        | U32_LSHIFT(1, 16)                     \
        | U32_LSHIFT(1, 26)                     \
    )

static inline uint64_t
__get_x_ctls_msr(const uint64_t true_msr, const uint64_t msr)
{
    return (VMX_DEFAULT1_NULLABLE()) ? read_msr(true_msr) : read_msr(msr);
}

#define get_pinbased_ctls_msr() \
    __get_x_ctls_msr(MSR_IA32_VMX_TRUE_PINBASED_CTLS, MSR_IA32_VMX_PINBASED_CTLS)

static inline void
set_reserved_pinbased_vm_execution_ctrl_fields(union vmcs_pin_based_ctls_vector *pin_vector)
{
    const uint64_t pinbased_ctls = get_pinbased_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(pinbased_ctls);
    const uint32_t allowed1 = U64_UPPER32(pinbased_ctls);

    pin_vector->raw |= (allowed0 | U32(PINBASED_CTLS_DEFAULT1_MASK));
    pin_vector->raw &= allowed1;
}

static inline void
configure_pin_based_vm_execution_ctls(vcpu_t *vcpu)
{
    union vmcs_pin_based_ctls_vector pin_vector;
    pin_vector.raw = 0;
    set_reserved_pinbased_vm_execution_ctrl_fields(&pin_vector);

    pin_vector.external_interrupt_handling = PIN_BASED_CTL_DEACTIVATE;
    pin_vector.nmi_exiting = PIN_BASED_CTL_DEACTIVATE;
    pin_vector.virtual_nmis = PIN_BASED_CTL_DEACTIVATE;
    pin_vector.activate_vmx_preemption_timer = PIN_BASED_CTL_DEACTIVATE;
    pin_vector.process_posted_interrupts = PIN_BASED_CTL_DEACTIVATE;

    const uint32_t old_pin_vector_raw = pin_vector.raw;
    set_reserved_pinbased_vm_execution_ctrl_fields(&pin_vector);

    if (old_pin_vector_raw != pin_vector.raw) {
        pr_warn("Reserved fields in the pin-based control vector were configured");
    }

    vmx_write_doubleword(vcpu, PIN_BASED_VM_EXECUTION_CONTROLS, pin_vector.raw);
}

#define get_primary_procbased_ctls_msr() \
    __get_x_ctls_msr(MSR_IA32_VMX_TRUE_PROCBASED_CTLS, MSR_IA32_VMX_PROCBASED_CTLS)

static inline void
set_reserved_primary_cpu_execution_ctrl_fields(union vmcs_primary_processor_based_ctls_vector *ctrl_vector)
{
    const uint64_t procbased_ctls = get_primary_procbased_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(procbased_ctls);
    const uint32_t allowed1 = U64_UPPER32(procbased_ctls);

    ctrl_vector->raw |= (allowed0 | U32(PRIMARY_PROCBASED_CTLS_DEFAULT1_MASK));
    ctrl_vector->raw &= allowed1;
}

static inline void
configure_processor_based_execution_ctls(vcpu_t *vcpu)
{
    /**
     * For now we only use the primary controls, and even there disable everything
     * possible.
     */
    union vmcs_primary_processor_based_ctls_vector ctrl_vector;
    ctrl_vector.raw = 0;
    set_reserved_primary_cpu_execution_ctrl_fields(&ctrl_vector);

    vmx_write_doubleword(vcpu, PRIMARY_PROC_BASED_VM_EXEC_CONTROLS, ctrl_vector.raw);
}

static inline void
setup_vm_execution_control_fields(vcpu_t *vcpu)
{
    configure_pin_based_vm_execution_ctls(vcpu);
    configure_processor_based_execution_ctls(vcpu);
}

/******************************************************************************
 * VM-exit control field setup
******************************************************************************/

#define PRIMARY_VM_EXIT_CTLS_DEFAULT1_MASK      \
    (                                           \
        U32_LSHIFT(1, 0)                        \
        | U32_LSHIFT(1, 1)                      \
        | U32_LSHIFT(1, 2)                      \
        | U32_LSHIFT(1, 3)                      \
        | U32_LSHIFT(1, 4)                      \
        | U32_LSHIFT(1, 5)                      \
        | U32_LSHIFT(1, 6)                      \
        | U32_LSHIFT(1, 7)                      \
        | U32_LSHIFT(1, 8)                      \
        | U32_LSHIFT(1, 10)                     \
        | U32_LSHIFT(1, 11)                     \
        | U32_LSHIFT(1, 13)                     \
        | U32_LSHIFT(1, 14)                     \
        | U32_LSHIFT(1, 16)                     \
        | U32_LSHIFT(1, 17)                     \
     )

#define get_primary_vm_exit_ctls_msr()          \
    __get_x_ctls_msr(MSR_IA32_VMX_TRUE_EXIT_CTLS, MSR_IA32_VMX_EXIT_CTLS)

static inline void
set_reserved_primary_vm_exit_ctl_fields(union vmcs_primary_vm_exit_ctls_vector *ctrl_vector)
{
    const uint64_t exit_ctls = get_primary_vm_exit_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(exit_ctls);
    const uint32_t allowed1 = U64_UPPER32(exit_ctls);

    ctrl_vector->raw |= (allowed0 | U32(PRIMARY_VM_EXIT_CTLS_DEFAULT1_MASK));
    ctrl_vector->raw &= allowed1;
}

static inline void
configure_vm_exit_controls(vcpu_t *vcpu)
{
    /**
     * For now we only use the primary vm-exit controls and we disable everything
     * possible
     */
    union vmcs_primary_vm_exit_ctls_vector ctls_vector;
    ctls_vector.raw = 0;
    set_reserved_primary_vm_exit_ctl_fields(&ctls_vector);

    ctls_vector.host_addr_space_size = 1;

    const uint32_t old_ctls_vector_raw = ctls_vector.raw;
    set_reserved_primary_vm_exit_ctl_fields(&ctls_vector);

    if (old_ctls_vector_raw != ctls_vector.raw) {
        pr_warn("Reserved fields in the primary vm-exit control vector were configured");
    }

    vmx_write_doubleword(vcpu, PRIMARY_VM_EXIT_CONTROLS, ctls_vector.raw);
}

/******************************************************************************
 * VM-entry control field setup
******************************************************************************/

#define VM_ENTRY_CTLS_DEFAULT1_MASK             \
    (                                           \
        U32_LSHIFT(1, 0)                        \
        | U32_LSHIFT(1, 1)                      \
        | U32_LSHIFT(1, 2)                      \
        | U32_LSHIFT(1, 3)                      \
        | U32_LSHIFT(1, 4)                      \
        | U32_LSHIFT(1, 5)                      \
        | U32_LSHIFT(1, 6)                      \
        | U32_LSHIFT(1, 7)                      \
        | U32_LSHIFT(1, 8)                      \
        | U32_LSHIFT(1, 12)                     \
     )

#define get_vm_entry_ctls_msr()                 \
    __get_x_ctls_msr(MSR_IA32_VMX_TRUE_ENTRY_CTLS, MSR_IA32_VMX_ENTRY_CTLS)

static inline void
set_reserved_vm_entry_ctl_fields(union vmcs_vm_entry_ctls_vector *ctrl_vector)
{
    const uint64_t entry_ctls = get_vm_entry_ctls_msr();
    const uint32_t allowed0 = U64_LOWER32(entry_ctls);
    const uint32_t allowed1 = U64_UPPER32(entry_ctls);

    ctrl_vector->raw |= (allowed0 | U32(VM_ENTRY_CTLS_DEFAULT1_MASK));
    ctrl_vector->raw &= allowed1;
}

static inline void
configure_vm_entry_controls(vcpu_t *vcpu)
{
    union vmcs_vm_entry_ctls_vector ctls_vector;
    ctls_vector.raw = 0;
    set_reserved_vm_entry_ctl_fields(&ctls_vector);

    ctls_vector.ia32e_mode_guest = 1;

    const uint32_t old_ctls_vector_raw = ctls_vector.raw;
    set_reserved_vm_entry_ctl_fields(&ctls_vector);

    if (old_ctls_vector_raw != ctls_vector.raw) {
        pr_warn("Reserved fields in the vm-entry control vector were configured");
    }

    vmx_write_doubleword(vcpu, VM_ENTRY_CONTROLS, ctls_vector.raw);
}

/******************************************************************************
 * VM setup
******************************************************************************/

void
arch_create_vm_from_guest_config(const guest_cfg_t *config)
{
    vcpu_t *vcpu = (vcpu_t *) hmalloc(sizeof(vcpu_t));
    if (vcpu == NULL) {
        die_reason("Failed to allocate vcpu struct");
    }

    init_vcpu(vcpu);

    /*setup_guest_state(vcpu);*/
    setup_host_state_area(vcpu);
    setup_vm_execution_control_fields(vcpu);
    configure_vm_exit_controls(vcpu);
    configure_vm_entry_controls(vcpu);

    try_entry(vcpu);
}

