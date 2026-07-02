.extern apic_signal_eoi

.macro def_global_function name
    .global \name
    .type \name, @function
.endm

.macro build_stack_frame
    pushq %rbp
    movq %rsp, %rbp
.endm

.macro save_registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro restore_registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

.macro asm_irq_handler_with_eoi name, cfunc
    def_global_function \name
    \name:
        build_stack_frame
        save_registers

        callq \cfunc

        callq apic_signal_eoi

        restore_registers
        leave
        iretq
.endm

.macro asm_irq_handler_without_eoi name, cfunc
    def_global_function \name
    \name:
        build_stack_frame
        save_registers

        callq \cfunc

        restore_registers
        leave
        iretq
.endm

.macro asm_exception_handler_without_err name, cfunc
    def_global_function \name
    \name:
        build_stack_frame
        save_registers

        callq \cfunc

        restore_registers
        leave
        iretq
.endm

.section .text

# Generate the interrupt and exception handler entry points
#include "asm/idt.h"

