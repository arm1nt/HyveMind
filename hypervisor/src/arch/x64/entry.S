.SECTION .text

.macro idt_asm_trampoline name, cfunc
    .global \name
    .type \name, @function

    \name:
        push %rbp
        mov %rsp,%rbp

        call \cfunc

        pop %rbp
        iretq
.endm

# Generate the asm trampoline calls for the interrupt handlers
#include "idt_new.h"

