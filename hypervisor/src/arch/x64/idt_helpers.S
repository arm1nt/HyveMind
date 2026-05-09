.SECTION .text

.global reload_cs_register
.type reload_cs_register, @function

# Requires a segment selector as only argument
reload_cs_register:
    push %rbp
    mov %rsp,%rbp

    push %rdi # target cs selector
    push 8(%rbp) # return instruction pointer
    movq (%rbp),%rbp
    # Pop the return instruction pointer pushed by 'call' and the pushed rbp
    lretq $16

