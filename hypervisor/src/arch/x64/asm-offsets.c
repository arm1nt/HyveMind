#include "asm/vm.h"
#include "asm/vcpu_arch_state.h"

#define DEFINE_ASM_OFFSET(sym, offset)  \
    asm volatile("\n.ascii \"-->> #define " #sym " %c0 \"\n" :: "i" (offset))

#define OFFSET(name, _struct, member)   \
    DEFINE_ASM_OFFSET(name, offsetof(_struct, member))

void
__dummy_vcpu_user_regs_offsets(void)
{
    OFFSET(USREG_R15, struct vcpu_user_regs, r15);
    OFFSET(USREG_R14, struct vcpu_user_regs, r14);
    OFFSET(USREG_R13, struct vcpu_user_regs, r13);
    OFFSET(USREG_R12, struct vcpu_user_regs, r12);
    OFFSET(USREG_R11, struct vcpu_user_regs, r11);
    OFFSET(USREG_R10, struct vcpu_user_regs, r10);
    OFFSET(USREG_R9, struct vcpu_user_regs, r9);
    OFFSET(USREG_R8, struct vcpu_user_regs, r8);
    OFFSET(USREG_RAX, struct vcpu_user_regs, rax);
    OFFSET(USREG_RBX, struct vcpu_user_regs, rbx);
    OFFSET(USREG_RCX, struct vcpu_user_regs, rcx);
    OFFSET(USREG_RDX, struct vcpu_user_regs, rdx);
    OFFSET(USREG_RSI, struct vcpu_user_regs, rsi);
    OFFSET(USREG_RDI, struct vcpu_user_regs, rdi);
    OFFSET(USREG_RBP, struct vcpu_user_regs, rbp);
    OFFSET(USREG_RIP, struct vcpu_user_regs, rip);
    OFFSET(USREG_RSP, struct vcpu_user_regs, rsp);
    OFFSET(USREG_RFLAGS, struct vcpu_user_regs, rflags);
}

void
__dummy_arch_vcpu_members_offset(void)
{
    OFFSET(AVCPU_ACTIVE, struct arch_vcpu, active);
    OFFSET(AVCPU_ACTIVE_CPU, struct arch_vcpu, active_processor);
    OFFSET(AVCPU_USR_STATE, struct arch_vcpu, state);

    OFFSET(AVCPU_HW, struct arch_vcpu, hw);
    OFFSET(AVCPU_HW_VMX_LAUNCH_STATE, struct vmx_vcpu_state, launch_state);
}
