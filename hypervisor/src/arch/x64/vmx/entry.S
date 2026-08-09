# todo: needs some changes (e.g. be able to return from the exit_handler) to
#   work in a scheduling loop.

.section .text

#include "vmx/vmcs.h"
#include "vmx/vmx_ops.h"
#include "asm/generated/asm-offsets.h"

.extern __vmx_entry_helper
.extern vmx_vm_exit_handler
.extern __printf

.macro def_global_func name
    .global \name
    .type \name, @function
.endm

.macro setup_stack_frame
    pushq %rbp
    movq %rsp, %rbp
.endm

.macro push_preserved_regs
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
.endm

.macro pop_preserved_regs
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
.endm

.macro load_guest_regs ptr_reg
    movq USREG_R15(%\ptr_reg), %r15
    movq USREG_R14(%\ptr_reg), %r14
    movq USREG_R13(%\ptr_reg), %r13
    movq USREG_R12(%\ptr_reg), %r12
    movq USREG_R11(%\ptr_reg), %r11
    movq USREG_R10(%\ptr_reg), %r10
    movq USREG_R9(%\ptr_reg), %r9
    movq USREG_RAX(%\ptr_reg), %rax
    movq USREG_RBX(%\ptr_reg), %rbx
    movq USREG_RCX(%\ptr_reg), %rcx
    movq USREG_RDX(%\ptr_reg), %rdx
    movq USREG_RSI(%\ptr_reg), %rsi
    movq USREG_RDI(%\ptr_reg), %rdi
    movq USREG_RBP(%\ptr_reg), %rbp
    # We store the ptr to the vcpu_user_regs struct in r8, so we need to
    # overwrite it last.
    movq USREG_R8(%\ptr_reg), %r8
.endm

.macro save_and_clear_reg _ptr_reg _reg _offset
    movq %\_reg, \_offset(%\_ptr_reg)
    xor %\_reg, %\_reg
.endm

.macro save_guest_regs ptr_reg
    save_and_clear_reg \ptr_reg, r15, USREG_R15
    save_and_clear_reg \ptr_reg, r14, USREG_R14
    save_and_clear_reg \ptr_reg, r13, USREG_R13
    save_and_clear_reg \ptr_reg, r12, USREG_R12
    save_and_clear_reg \ptr_reg, r11, USREG_R11
    save_and_clear_reg \ptr_reg, r10, USREG_R10
    save_and_clear_reg \ptr_reg, r9,  USREG_R9
    save_and_clear_reg \ptr_reg, r8,  USREG_R8
    save_and_clear_reg \ptr_reg, rax, USREG_RAX
    save_and_clear_reg \ptr_reg, rbx, USREG_RBX
    save_and_clear_reg \ptr_reg, rcx, USREG_RCX
    save_and_clear_reg \ptr_reg, rdx, USREG_RDX
    save_and_clear_reg \ptr_reg, rsi, USREG_RSI
    # skip rdi as we store the struct ptr in it -> we restore it later
    save_and_clear_reg \ptr_reg, rbp, USREG_RBP
    # we read the guest RIP, RSP and RFLAGS from the vmcs
.endm

def_global_func asm_vmx_vm_exit_handler
asm_vmx_vm_exit_handler:
    # Make one register available to store the user regs struct ptr
    pushq %rdi
    movq +0x08(%rsp), %rdi

    save_guest_regs rdi
    movq (%rsp), %r8
    save_and_clear_reg rdi, r8, USREG_RDI

    # Starting from here all guest state is saved.
    addq $0x08, %rsp
    movq %rsp, %rbp

    movq (%rsp), %rdi
    callq vmx_vm_exit_handler

    # Atm, this point should be unreachable since we explicitly call do_enter_vcpu
    # in the c exit handler.
    leaq log_exit_unreachable(%rip), %rdi
    callq __printf
    hlt

# int func(vcpu_t*, int launch_state, struct vcpu_user_regs*)
def_global_func asm_do_vmx_vcpu_entry
asm_do_vmx_vcpu_entry:
    setup_stack_frame

    pushq %rbp
    push_preserved_regs

    movq %rdi, %r12     # pointer to vcpu struct
    movq %rsi, %r13     # launch state value
    movq %rdx, %r14     # ptr to vcpu_user_regs struct

    movq %r12, %rdi
    callq __vmx_entry_helper
    movq %rax, %rbx
    movq $-0x01, %rax
    test $0x1, %rbx
    jz .Lvm_entry_fail

    cmp $VMCS_LS_CLEAR_VAL, %r13d
    je .Lvmlaunch_op
    jmp .Lvmresume_op

.Lvmlaunch_op:
    movq %r14, %r8
    load_guest_regs r8

    vmlaunch
    jmp .Lvm_entry_op_fail

.Lvmresume_op:
    movq %r14, %r8
    load_guest_regs r8

    vmresume

.Lvm_entry_op_fail:
    pushfq
    popq %rdi
    callq eflags_to_vmx_op_status

.Lvm_entry_fail:
    pop_preserved_regs
    popq %rbp

    leave
    retq

.section .data

log_exit_unreachable:
    .asciz "Reached should-be unreachable point after vm exit handler call"

