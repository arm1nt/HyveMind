#include "vm.h"
#include "asm/irq_vectors.h"
#include "asm/processor.h"
#include "vmx/emulate.h"
#include "vmx/vmx.h"
#include "vmx/vmx_ops.h"
#include "vmx/vmcs.h"

static const char hyvemind_id[12] = "hyvemind   ";

static void
emulate_cpuid(vcpu_t *vcpu, struct vcpu_user_regs *regs)
{
    uint64_t instruction_length;
    cpuid_result_t cpuid_res;
    const uint64_t leaf = regs->rax;
    const uint64_t sub_leaf = regs->rcx;

    switch (leaf) {
        case 0x01:
            cpuid_res = cpuid_raw(leaf, sub_leaf);
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

    vmread(VM_EXIT_INSTRUCTION_LENGTH, &instruction_length);
    regs->rip += instruction_length;
}

static void
emulate_rdmsr(vcpu_t *vcpu, struct vcpu_user_regs *regs)
{
    const uint64_t msr = regs->rcx;

    /**
     * We currently let a guest access default MSRs without a VM-exit, but non-
     * default MSRs we have to handle separately.
     */

    switch (msr) {
        default:
            vmx_inject_exception(vcpu, IRQ_GP_VECTOR, 0);
    }
}

static void
emulate_xsetbv(vcpu_t *vcpu, struct vcpu_user_regs *regs)
{
    uint64_t instruction_length;

    if (regs->rcx != 0) {
        vmx_inject_exception(vcpu, IRQ_UD_VECTOR, 0);
        return;
    }

    /* Theoretically, we need more checks, e.g. do host&guest support xsave, etc. */

    asm volatile ("xsetbv" :: "a"(regs->rax), "d"(regs->rdx), "c"(regs->rcx));

    vmread(VM_EXIT_INSTRUCTION_LENGTH, &instruction_length);
    regs->rip += instruction_length;
}

struct x86_emulate_ops emulate_ops = {
    .emulate_cpuid = emulate_cpuid,
    .emulate_xsetbv = emulate_xsetbv,
    .emulate_rdmsr = emulate_rdmsr,
};

