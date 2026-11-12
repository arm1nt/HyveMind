#include "string.h"
#include "asm/cpufeatures.h"
#include "asm/gdt_idt.h"
#include "asm/processor.h"
#include "asm/segmentation.h"
#include "vm/vcpu.h"
#include "vm/vlapic.h"
#include "vmx/vmcs.h"
#include "vmx/vmx.h"
#include "vmx/vmx_ops.h"

static inline vmcs_ar_t
get_unusable_ar(void)
{
    vmcs_ar_t unusable_ar;
    unusable_ar.raw = 0;
    unusable_ar.segment_unusable = 1;
    return unusable_ar;
}

static inline void
reset_segments_to_init_state(struct vcpu_guest_reg_state *state)
{
    struct vcpu_segments *segments = &state->segments;

    vmcs_ar_t cs_ar;
    cs_ar.raw = 0;
    cs_ar.present = SEGMENT_PRESENT;
    cs_ar.descriptor_type = CODE_DATA_SEGMENT_DESC;
    cs_ar.segment_type = CODE_EXECUTE_READ_ACCESSED;

    segments->cs = DEFINE_VCPU_SEG(0xF000, 0xFFFF0000, 0xFFFF, cs_ar);

    vmcs_ar_t seg_ar;
    seg_ar.raw = 0;
    seg_ar.present = SEGMENT_PRESENT;
    seg_ar.descriptor_type = CODE_DATA_SEGMENT_DESC;
    seg_ar.segment_type = DATA_RW_ACCESSED;

    segments->ss = DEFINE_VCPU_SEG(0, 0, 0xFFFF, seg_ar);
    segments->ds = DEFINE_VCPU_SEG(0, 0, 0xFFFF, seg_ar);
    segments->es = DEFINE_VCPU_SEG(0, 0, 0xFFFF, seg_ar);
    segments->fs = DEFINE_VCPU_SEG(0, 0, 0xFFFF, seg_ar);
    segments->gs = DEFINE_VCPU_SEG(0, 0, 0xFFFF, seg_ar);

    vmcs_ar_t ldtr_ar;
    ldtr_ar.raw = 0;
    ldtr_ar.present = SEGMENT_PRESENT;
    ldtr_ar.descriptor_type = SYSTEM_SEGMENT_DESC;
    ldtr_ar.segment_type = IA32E_LDT;

    segments->ldtr = DEFINE_VCPU_SEG(0, 0, 0xFFFF, ldtr_ar);

    vmcs_ar_t tr_ar;
    tr_ar.raw = 0;
    tr_ar.present = SEGMENT_PRESENT;
    tr_ar.descriptor_type = SYSTEM_SEGMENT_DESC;
    tr_ar.segment_type = IA32E_TSS_BUSY;

    segments->tr = DEFINE_VCPU_SEG(0, 0, 0xFFFF, tr_ar);
}

void
reset_guest_reg_state_to_init_state(struct vcpu_guest_reg_state *state)
{
    memset(state, 0, sizeof(struct vcpu_guest_reg_state));

    state->cr0.et = 1;
    state->cr0.cd = 1;
    state->cr0.nw = 1;
    state->cr0 = sanitize_cr0_for_vmx_operation(state->cr0);

    state->cr4 = sanitize_cr4_for_vmx_operation(state->cr4);

    state->dr7 = 0x400;
    state->uregs.eflags = 0x02;
    state->uregs.eip = 0xFFF0;

    reset_segments_to_init_state(state);

    state->gdtr.base = 0;
    state->gdtr.limit = 0xFFFF;
    state->idtr.base = 0;
    state->idtr.limit = 0xFFFF;
}

static inline void
mirror_host_ctrl_regs(struct vcpu_guest_reg_state *state)
{
    state->cr0.raw = read_cr0();
    state->cr3.cr3_64b.raw = read_cr3();
    state->cr4.raw = read_cr4();
}

static inline vmcs_ar_t
get_current_tr_ar(void)
{
    vmcs_ar_t ar;
    ar.raw = 0;
    ar.segment_type = IA32E_TSS_BUSY;
    ar.descriptor_type = SYSTEM_SEGMENT_DESC;
    ar.present = SEGMENT_PRESENT;
    return ar;
}

static inline vmcs_ar_t
get_current_cs_ar(void)
{
    vmcs_ar_t ar;
    ar.raw = 0;
    ar.segment_type = CODE_EXECUTE_ONLY_ACCESSED;
    ar.descriptor_type = CODE_DATA_SEGMENT_DESC;
    ar.present = SEGMENT_PRESENT;
    ar.l = 1;
    return ar;
}

static inline void
mirror_host_segment_registers(struct vcpu_guest_reg_state *state)
{
    struct vcpu_segments *segments = &state->segments;

    segments->cs.selector = read_segment_register(X86_CS_REG);
    segments->tr.selector = read_task_register();

    segments->tr.base = get_current_tss_base();
    segments->tr.limit = U64(sizeof(tss_t) - 1);

    vmcs_ar_t unusable_ar = get_unusable_ar();

    segments->cs.access_rights = get_current_cs_ar();
    segments->ss.access_rights = unusable_ar;
    segments->ds.access_rights = unusable_ar;
    segments->es.access_rights = unusable_ar;
    segments->fs.access_rights = unusable_ar;
    segments->gs.access_rights = unusable_ar;
    segments->tr.access_rights = get_current_tr_ar();
    segments->ldtr.access_rights = unusable_ar;
}

void
init_guest_reg_state_mirroring_host(struct vcpu_guest_reg_state *state)
{
    memset(state, 0, sizeof(struct vcpu_guest_reg_state));

    mirror_host_ctrl_regs(state);
    state->dr7 = read_dr7();
    state->efer = read_efer();

    state->uregs.rflags = 0x02;

    mirror_host_segment_registers(state);

    const gdt_ptr_t gdtr = read_gdtr();
    const idt_ptr_t idtr = read_idtr();

    state->gdtr.base = gdtr.base;
    state->gdtr.limit = gdtr.limit;
    state->idtr.base = idtr.base;
    state->idtr.limit = idtr.limit;
}

static void
init_segments_for_linux_32bit(struct vcpu_segments *segments)
{
    vmcs_ar_t tr_ar;
    const vmcs_ar_t unusable_ar = get_unusable_ar();

    segments->cs.base = 0;
    segments->cs.limit = 0xFFFFFFFF;
    segments->cs.raw_selector = 0x10;
    segments->cs.access_rights.raw = 0xC09B;

    segments->ds.base = 0;
    segments->ds.limit = 0xFFFFFFFF;
    segments->ds.raw_selector = 0x18;
    segments->ds.access_rights.raw = 0xc093;

    segments->es = segments->ds;
    segments->ss = segments->ds;

    segments->gs.access_rights = unusable_ar;
    segments->fs.access_rights = unusable_ar;
    segments->ldtr.access_rights = unusable_ar;

    tr_ar.raw = 0;
    tr_ar.present = SEGMENT_PRESENT;
    tr_ar.descriptor_type = SYSTEM_SEGMENT_DESC;
    tr_ar.segment_type = IA32E_TSS_BUSY;
    segments->tr = DEFINE_VCPU_SEG(0, 0, 0xFFFF, tr_ar);
}

void
init_guest_reg_state_for_linux_32bit(struct vcpu_guest_reg_state *state)
{
    memset(state, 0, sizeof(struct vcpu_guest_reg_state));

    /**
     * Sanitization forces pg & pe to be enabled.
     */
    state->cr0 = sanitize_cr0_for_vmx_operation(state->cr0);
    state->cr0.pe = 1;
    state->cr0.pg = 0;

    state->cr4 = sanitize_cr4_for_vmx_operation(state->cr4);

    state->uregs.eflags = 0x02;

    init_segments_for_linux_32bit(&state->segments);

    state->gdtr.limit = 0xFF;
}

static const char hyvemind_id[12] = "hyvemind   ";

void
vcpu_step_rip(vcpu_t *vcpu)
{
    uint64_t instr_length;
    vmread(VM_EXIT_INSTRUCTION_LENGTH, &instr_length);
    vcpu_state(vcpu).user_regs.rip += instr_length;
}

static inline void
sanitize_initial_apic_id(vcpu_t *vcpu, cpuid_result_t *cpuid_val)
{
    const struct vlapic *vlapic = vcpu_vlapic(vcpu);
    const uint32_t clear_id_bits_mask = ~(U32_LSHIFT(0xFF, 24));
    cpuid_val->ebx &= clear_id_bits_mask;
    cpuid_val->ebx |= U32_LSHIFT(vlapic->physical_apic_id, 24);
}

void
vcpu_handle_cpuid(vcpu_t *vcpu)
{
    struct vcpu_user_regs *regs = &vcpu_state(vcpu).user_regs;
    const uint64_t leaf = regs->rax;
    const uint64_t sub_leaf = regs->rcx;
    cpuid_result_t cpuid_res;

    switch (leaf) {
        case CPUID_VERSION_FEATURES_LEAF:
            cpuid_res = cpuid_raw(leaf, sub_leaf);
            sanitize_initial_apic_id(vcpu, &cpuid_res);
            break;
        case CPUID_EXT_TOPOLOGY_LEAF:
        case CPUID_V2_EXT_TOPOLOGY_LEAF:
            /* For now we simply pass through info other than the apic id */
            cpuid_res = cpuid_raw(leaf, sub_leaf);
            cpuid_res.edx = vcpu_vlapic(vcpu)->physical_apic_id;
            break;
        case 0x40000000:
            memset(&cpuid_res, 0, sizeof(cpuid_result_t));
            memcpy(&cpuid_res.ebx, &hyvemind_id[0], 4);
            memcpy(&cpuid_res.ecx, &hyvemind_id[4], 4);
            memcpy(&cpuid_res.edx, &hyvemind_id[8], 4);
            break;
        default:
            cpuid_res = cpuid_raw(leaf, sub_leaf);
    }

    regs->rax = cpuid_res.eax;
    regs->rbx = cpuid_res.ebx;
    regs->rcx = cpuid_res.ecx;
    regs->rdx = cpuid_res.edx;
    vcpu_step_rip(vcpu);
}

void
vcpu_handle_rdmsr(vcpu_t *vcpu)
{
    struct vcpu_user_regs *regs = &vcpu_state(vcpu).user_regs;
    const uint64_t msr = regs->rcx;

    /**
     * Default MSRs can be accessed without VM-exit, but certain MSRs need
     * extra handling:
     */
    switch (msr) {
        case MSRX64_IA32_APIC_BASE:
            regs->rax = U64_LOWER32(vcpu_vlapic(vcpu)->apic_base_msr.raw);
            regs->rdx = U64_UPPER32(vcpu_vlapic(vcpu)->apic_base_msr.raw);
            break;
        default:
            vmx_inject_exception(vcpu, IRQ_GP_VECTOR, 0);
            return;
    }

    vcpu_step_rip(vcpu);
}

void
vcpu_handle_xsetbv(vcpu_t *vcpu)
{
    struct vcpu_user_regs *regs = &vcpu_state(vcpu).user_regs;

    if (regs->rcx != 0) {
        vmx_inject_exception(vcpu, IRQ_UD_VECTOR, 0);
        return;
    }

    /* Theoretically, we need more checks, e.g. do host&guest support xsave, etc. */
    asm volatile ("xsetbv" :: "a"(regs->rax), "d"(regs->rdx), "c"(regs->rcx));
    vcpu_step_rip(vcpu);
}

