#ifndef _HYVEMIND_X64_VMX_EMULATE_H
#define _HYVEMIND_X64_VMX_EMULATE_H

struct vcpu;
struct vcpu_user_regs;

struct x86_emulate_ops {
    void (*emulate_cpuid) (struct vcpu *vcpu, struct vcpu_user_regs *regs);
    void (*emulate_rdmsr) (struct vcpu *vcpu, struct vcpu_user_regs *regs);
    void (*emulate_xsetbv) (struct vcpu *vcpu, struct vcpu_user_regs *regs);
};

extern struct x86_emulate_ops emulate_ops;

#endif /* _HYVEMIND_X64_VMX_EMULATE_H */

