#include "pf_alloc.h"
#include "printf.h"
#include "vm.h"
#include "asm/gdt_idt.h"
#include "asm/segmentation.h"

static inline void
mirror_ctrl_registers(struct vcpu_guest_arch_state *state)
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
mirror_segment_registers(struct vcpu_guest_arch_state *state)
{
    struct vcpu_segments *segments  = &state->segments;
    memset(segments, 0, sizeof(struct vcpu_segments));

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
vcpu_guest_mirror_current_cpu(struct vcpu_guest_arch_state *state)
{
    mirror_ctrl_registers(state);
    state->dr7 = 0;
    state->efer = read_efer();

    memset(&state->uregs, 0, sizeof(struct vcpu_user_regs));
    state->uregs.rflags = 0x02;

    mirror_segment_registers(state);

    const gdt_ptr_t gdtr = read_gdtr();
    const idt_ptr_t idtr = read_idtr();

    state->gdtr.base = gdtr.base;
    state->gdtr.limit = gdtr.limit;
    state->idtr.base = idtr.base;
    state->idtr.limit = idtr.limit;
}

static inline void
vcpu_guest_reset_segments_to_init_state(struct vcpu_guest_arch_state *state)
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
vcpu_guest_reset_to_init_state(struct vcpu_guest_arch_state *state)
{
    state->cr0.raw = 0;
    state->cr0.et = 1;
    state->cr0.cd = 1;
    state->cr0.nw = 1;

    state->cr3.cr3_64b.raw = 0;
    state->cr4.raw = 0;
    state->dr7 = 0x400;
    state->efer.raw = 0;

    memset(&state->uregs, 0, sizeof(struct vcpu_user_regs));
    state->uregs.eflags = 0x02;
    state->uregs.eip = 0xFFF0;

    vcpu_guest_reset_segments_to_init_state(state);

    state->gdtr.base = 0;
    state->gdtr.limit = 0xFFFF;
    state->idtr.base = 0;
    state->idtr.limit = 0xFFFF;
}

int
vcpu_guest_allocate_stack(struct vcpu_guest_arch_state *state, const int nr_pages)
{
    virt_addr_t stack_bot;
    if (get_pages_zeroed(nr_pages, &stack_bot) != 0) {
        pr_error("Failed to allocate '%lu' pages for the guest stack.", U64(nr_pages));
        return -1;
    }

    state->uregs.rsp = (stack_bot + (nr_pages * PAGE_SIZE)) - 8;
    return 0;
}

