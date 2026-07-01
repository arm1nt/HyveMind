#ifndef _HYVEMIND_X64_ASM_IDT_H
#define _HYVEMIND_X64_ASM_IDT_H

#ifndef __ASSEMBLER__

#include "asm/irq_vectors.h"

#define DECLARE_IDT_ASM_TRAMPOLINE(name, cfunc) extern void name(void);

#else /* __ASSEMBLER__ */

#define DECLARE_IDT_ASM_TRAMPOLINE(name, cfunc) \
    idt_asm_trampoline name, cfunc

#endif /* __ASSEMBLER__ */

/**
 * In the assembly file, generate the asm trampoline.
 * For C files, declare references to the assembly functions
 */

DECLARE_IDT_ASM_TRAMPOLINE(asm_div_exception_handler, c_div_exception_handler)
DECLARE_IDT_ASM_TRAMPOLINE(asm_double_fault_handler, c_double_fault_handler)

#endif /* _HYVEMIND_X64_ASM_IDT_H */

