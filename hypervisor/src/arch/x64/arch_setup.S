.extern __arch_setup_bsp
.extern get_pages
.extern memcpy
.extern __printf

.section .data

# Scratch space for storing the memory map, executable info, etc.
.global boot_mem_info_scratch
boot_mem_info_scratch:
    .fill 4096, 1, 0

.global boot_info_scratch
boot_info_scratch:
    .fill 4096, 1, 0

.section .text

.macro build_stack_frame
    pushq %rbp
    movq %rsp, %rbp
.endm

# void do_unwind_and_rebase(start_of_stack, nr_frames)
do_unwind_and_rebase:
    build_stack_frame

    # We need to also rebase this & the 'arch_rebase_curr_stack' frames
    add $0x1, %rsi
    movq %rsi, %rcx

    movq %rbp, %r10
    pushq %r10

unwind_next_frame:
    movq (%r10), %r10
    pushq %r10

    subq $0x1, %rcx
    cmpq $0x0, %rcx
    jne unwind_next_frame

    # At this point, we have the rbp's from all the frames we want to unwind
    # on the stack

    movq %rsi, %rcx     # Nr of rebase steps
    movq $0x0, %r13     # previous rbp value for the new stack
    movq %rdi, %r14     # curr rbp on the new stack
    movq %rdi, %r15     # curr rsp on the new stack

rebase_next_frame:
    popq %r10           # Base of the current frame
    movq (%rsp), %r11   # Base of the next frame

    # Compute the size of the frame and store in %rdx to be passed to memcpy
    movq %r10, %rdx
    subq %r11, %rdx

    # Compute the rsp value that the to-be-copied frame should have on the new
    # stack
    movq %r14, %r15
    subq %rdx, %r15
    addq $0x8, %r15
    movq %r15, %rdi

    # Compute the memory location in the current stack area where we should start
    # copying the frame
    movq %r11, %rsi
    addq $0x8, %rsi

    pushq %rcx
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    call memcpy

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %rcx

    # Update the rbp stored in the copied frame on the new stack to point to the
    # corresponding/correct position in the new stack area.
    movq %r13, (%r14)

    # prev_rbp = curr_rbp
    movq %r14, %r13

    # Adjust the rsp in the new stack to point right above the copied frame
    movq %r15, %r14
    subq $0x8, %r14

    subq $0x1, %rcx
    cmpq $0x0, %rcx
    jne rebase_next_frame

    # Write the rip to the top of the new stack and do the switch-over
    movq +0x8(%rbp), %r8
    movq %r8, (%r15)
    movq %r13, %rbp
    movq %r15, %rsp
    ret

# virt_addr_t get_start_of_stack(pages_start_addr, nr_pages)
get_start_of_stack:
    build_stack_frame

    movq %rsi, %r8
    movq $4096, %rax
    mulq %r8
    sub $0x1, %rax
    add %rdi, %rax

    leave
    ret

.global arch_rebase_current_stack
.type arch_rebase_current_stack, @function

# virt_addr_t arch_rebase_current_stack(nr_pages, frames_to_unwind)
arch_rebase_current_stack:
    build_stack_frame

    subq $0x10, %rsp
    movq %rdi, -0x8(%rbp)   # nr of pages
    movq %rsi, -0x10(%rbp)  # frames to unwind

    # Allocate space to store the start addr of the allocated pages
    subq $0x8, %rsp

    movq -0x8(%rbp), %rdi
    movq %rsp, %rsi
    callq get_pages
    cmp $0x0, %rax
    jne stack_rebase_page_alloc_error

    leaq suc_alloc_pages_msg(%rip), %rdi
    callq __printf

    movq (%rsp), %rdi
    movq -0x8(%rbp), %rsi
    callq get_start_of_stack

    # Store the stack start addr
    pushq %rax

    leaq stack_start_location_msg(%rip), %rdi
    movq %rax, %rsi
    callq __printf

    movq (%rsp), %rdi
    movq -0x10(%rbp), %rsi
    callq do_unwind_and_rebase

    popq %rax
    leave
    ret

stack_rebase_page_alloc_error:
    leaq err_alloc_pages_msg(%rip), %rdi
    callq __printf
    movq $0x0, %rax
    leave
    ret

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
stack_start_location_msg: .asciz "Start of newly allocated stack is at %lx\n"

