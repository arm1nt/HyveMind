.section .text

.global vm_entry_test
.type vm_entry_test, @function

vm_entry_test:
    nop
    nop
    nop
    nop
    mov $0, %eax
    mov $0, %ecx
    cpuid
    nop
    nop
    nop
    jmp vm_entry_test

