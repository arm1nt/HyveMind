.macro def_global_fn name
    .global \name
    .type \name, @function
.endm

.macro def_local_fn name
    .type \name, @function
.endm

.macro push_callee_preserved_regs
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
.endm

.macro pop_callee_preserved_regs
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
.endm

.macro setup_stack_frame
    pushq %rbp
    movq %rsp, %rbp
.endm

.section .text

#include "vmx/vmcs.h"
#include "vmx/vmx_ops.h"
#include "asm/generated/asm-offsets.h"

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
    # Skip rdi as we store the struct ptr in it -> we save it later
    save_and_clear_reg \ptr_reg, rbp, USREG_RBP
    # We read the guest RIP, RSP and RFLAGS from the VMCS
.endm

.extern vmx_prepare_vcpu_execution
.extern vmx_set_exit_handler_rsp
.extern vmx_external_irqs_entry_helper
.extern __vmx_entry_helper
.extern vmx_vm_exit_handler

def_local_fn get_vcpu_launch_state
get_vcpu_launch_state:
    setup_stack_frame

    movq %rdi, %r10
    addq $VCPU_VMX_LS_OFFSET, %r10
    movl (%r10), %eax

    leave
    retq

.global asm_vmx_vm_exit_handler
asm_vmx_vm_exit_handler:
    # Make one register available to store the user regs struct ptr
    pushq %rdi
    movq +0x08(%rsp), %rdi

    save_guest_regs rdi
    popq %r8
    save_and_clear_reg rdi, r8, USREG_RDI

    # All vCPU regs are now saved and the stack is in the same state as when
    # entering the handler. We can now establish the stack frame
    movq %rsp, %rbp
    addq $0x10, %rbp

    # Call the C handler with the vCPU ptr as argument
    movq +0x08(%rsp), %rdi
    callq vmx_vm_exit_handler
    cmp $0x00, %rax
    je .Lexit_handler_success

    # Simply pass through the return value from the C handler
    leave
    retq

.Lexit_handler_success:
asm_vmx_vm_entry_handler:
    #Todo: Check where to update vCPU state and disable IRQs
    cli

    # Store the vcpu & regs ptrs in callee-preserved registers
    movq +0x08(%rsp), %r12
    movq (%rsp), %r13

    movq %r12, %rdi
    callq vmx_external_irqs_entry_helper

    movq %r12, %rdi
    callq __vmx_entry_helper
    cmp $0x00, %rax
    je 1f

    movq $VMX_ERROR, %rax
    leave
    retq
1:
    movq %r12, %rdi
    callq get_vcpu_launch_state
    movq %rax, %r10

    cmp $VMCS_LS_INVALID_VAL, %r10
    jne 1f
    movq $VMX_INVALID_VCPU_LS, %rax
    leave
    retq
1:
    cmp $VMCS_LS_CLEAR_VAL, %r10
    je .Lvmlaunch_op

.Lvmresume_op:
    movq %r13, %r8
    load_guest_regs r8

    vmresume

    movq $VMX_VMRESUME_FAILED, %rax
    leave
    retq
.Lvmlaunch_op:
    movq %r13, %r8
    load_guest_regs r8

    vmlaunch

    movq $VMX_VMLAUNCH_FAILED, %rax
    leave
    retq

def_local_fn __vmx_execute_vcpu
__vmx_execute_vcpu:
    setup_stack_frame

    pushq %rdi # vcpu ptr
    pushq %rsi # regs ptr

    # At this point, the stack frame looks as follows:
    # |--------------------------|
    # | rip of vmx_execute_vcpu |
    # | rbp of vmx_execute_vcpu |
    # | vcpu ptr                |
    # | user regs ptr           |
    # |--------------------------| <<-- Z ... ptr to store into the VMCS host RSP
    #
    # With this stack frame, the vm_exit handler (roughly) works like a function
    # invoked by 'vmx_execute_vcpu' that has the vcpu and user regs ptrs as local
    # variables.

    # Called with the vcpu ptr as argument
    callq vmx_prepare_vcpu_execution
    cmp $0x00, %rax
    je .Lvcpu_prep_success

    movq $VMX_ERROR, %rax
    leave
    retq
.Lvcpu_prep_success:
    movq %rsp, %rdi
    callq vmx_set_exit_handler_rsp

    # The stack is now in the same state as it will be established by the
    # vm exit handler
    jmp asm_vmx_vm_entry_handler

def_global_fn vmx_execute_vcpu
vmx_execute_vcpu:
    setup_stack_frame
    push_callee_preserved_regs

    callq __vmx_execute_vcpu
    # We pass the return value through to the caller

    pop_callee_preserved_regs
    leave
    retq

