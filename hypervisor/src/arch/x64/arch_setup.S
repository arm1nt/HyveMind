.extern __arch_setup_bsp
.extern get_pages
.extern memcpy
.extern __printf

.section .data

# Scratch space for storing the memory map, executable info, etc.
.global boot_mem_info_scratch
boot_mem_info_scratch:
    .fill 4096, 1, 0

.section .text

.macro build_stack_frame
    pushq %rbp
    movq %rsp, %rbp
.endm

.global arch_replace_stack
.type arch_replace_stack, @function

arch_replace_stack:
    # We intentionally don't create a stack frame here

    movq (%rsp), %r10

    # Allocate stack space to store the start addr of the allocated pages
    sub $0x08, %rsp

    movq %rsp, %rsi
    callq get_pages
    cmp $0x0, %rax
    jne alloc_pages_error

    # Compute last addr in the range of allocated pages as top of stack
    # (page_size * nr_pages) - 1
    movq $0x4, %r8
    movq $4096, %rax
    mulq %r8
    sub $0x1, %rax
    add (%rsp), %rax

    # Make rsp & rbp point to the new value
    movq %rax, %rsp
    movq %rsp, %rbp
    pushq %rax

    leaq suc_alloc_pages_msg(%rip), %rdi
    callq __printf

    popq %rax
    pushq %r10
    ret

alloc_pages_error:
    leaq err_alloc_pages_msg(%rip), %rdi
    callq __printf
    movq $0x0, %rax
    ret

.global arch_setup_bsp
.type arch_setup_bsp, @function

arch_setup_bsp:
    build_stack_frame

    callq __arch_setup_bsp

    leave
    ret

.section .data
err_alloc_pages_msg: .asciz "Failed to allocate pages\n"
suc_alloc_pages_msg: .asciz "Successfully allocated pages\n"

