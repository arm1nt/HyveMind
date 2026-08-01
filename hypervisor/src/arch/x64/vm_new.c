#include "fatal.h"
#include "halloc.h"
#include "printf.h"
#include "string.h"
#include "vm.h"
#include "vmx/vmx.h"

static inline void
reset_vcpu_seg_regs_to_processor_init_state(vcpu_t *vcpu)
{
    struct vcpu_segments *segments = &vcpu->arch.state.segments;

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

    vmcs_ar_t ldtr_tr_ar;
    ldtr_tr_ar.raw = 0;
    ldtr_tr_ar.present = SEGMENT_PRESENT;
    ldtr_tr_ar.descriptor_type = CODE_DATA_SEGMENT_DESC;
    ldtr_tr_ar.segment_type = DATA_RW;

    segments->ldtr = DEFINE_VCPU_SEG(0, 0, 0xFFFF, ldtr_tr_ar);
    segments->tr = DEFINE_VCPU_SEG(0, 0, 0xFFFF, ldtr_tr_ar);
}

static void
reset_vcpu_to_processor_init_state(vcpu_t *vcpu)
{
    vcpu->arch.state.cpu_mode = REAL_MODE_16B;

    cr0_t user_cr0;
    user_cr0.raw = 0;
    user_cr0.et = 1;
    user_cr0.cd = 1;
    user_cr0.nw = 1;
    vcpu->arch.state.cr0 = user_cr0;

    cr3_t user_cr3;
    user_cr3.cr3_64b.raw = 0;
    vcpu->arch.state.cr3 = user_cr3;

    cr4_t user_cr4;
    user_cr4.raw = 0;
    vcpu->arch.state.cr4 = user_cr4;

    vcpu->arch.state.dr7 = 0x400;

    ia32_efer_t efer;
    efer.raw = 0;
    vcpu->arch.state.efer = efer;

    struct vcpu_user_regs *user_regs = &vcpu->arch.state.user_regs;
    memset(user_regs, 0, sizeof(struct vcpu_user_regs));

    user_regs->eflags = 0x02;
    user_regs->eip = 0xFFF0;
    user_regs->ebp = 0;
    user_regs->esp = 0;

    reset_vcpu_seg_regs_to_processor_init_state(vcpu);

    struct vcpu_sys_table sys_table = {
        .base = 0,
        .limit = 0xFFFF
    };

    vcpu->arch.state.gdtr = sys_table;
    vcpu->arch.state.idtr = sys_table;
}

static void
state_init_unpaged_protected_mode(vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

static inline void
mirror_host_ctrl_registers(vcpu_t *vcpu)
{
    NOT_YET_IMPLEMENTED;
}

static int
prepare_vm_mirroring_vmm(vcpu_t *vcpu, const struct guest_config *config)
{
    NOT_YET_IMPLEMENTED;
}

static int
prepare_vcpu_linux_direct_boot_32bit(vcpu_t *vcpu, const struct guest_config *config)
{
    NOT_YET_IMPLEMENTED;
}

static int
init_bsp_arch_vcpu(vcpu_t *vcpu, const struct guest_config *config)
{
    switch (config->guest_type) {
        case LINUX_DIRECT_BOOT_32BIT:
            return prepare_vcpu_linux_direct_boot_32bit(vcpu, config);
        case MIRROR_VMM:
            return prepare_vm_mirroring_vmm(vcpu, config);
        default:
            pr_error("Unsupported/Unknown VM guest type configured");
            return -1;
    }

    die_reason("Unreachable");
}

static int
init_vcpu(vcpu_t *vcpu)
{
    int ret;

    ret = vmx_init_vcpu(vcpu);
    if (ret != 0) {
        pr_debug("Failed to initialize the vmcs area & launch state for the vcpu");
        return ret;
    }

    reset_vcpu_to_processor_init_state(vcpu);

    return 0;
}

int
arch_init_vm(struct vm *vm, const struct guest_config *config)
{
    int ret;

    for (unsigned int i = 0; i < vm->nr_vcpus; i++) {
        ret = init_vcpu(vm->vcpus[i]);
        if (ret != 0) {
            pr_error("Failed to initialize vcpu");
            return ret;
        }
    }

    ret = init_bsp_arch_vcpu(vm->vcpus[0], config);
    if (ret != 0) {
        pr_error("Failed to init BSP vcpu");
        return -1;
    }

    return 0;
}

int
allocate_arch_vcpu(struct vcpu *vcpu)
{
    vcpu->arch.hw.vmx.virt_policy = hmalloc(sizeof(struct vmx_virt_policy));
    if (!vcpu->arch.hw.vmx.virt_policy) {
        pr_error("Failed to allocate a virt policy struct");
        return -1;
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

