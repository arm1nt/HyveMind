#ifndef _HYVEMIND_X64_ASM_IDT_H
#define _HYVEMIND_X64_ASM_IDT_H

#ifndef __ASSEMBLER__

#include "asm/irq_vectors.h"

#define DECLARE_IRQ_ENTRY_WITH_EOI(name, cfunc) extern void name(void);
#define DECLARE_IRQ_ENTRY_WITHOUT_EOI(name, cfunc) extern void name(void);
#define DECLARE_EXCEPTTION_HANDLER_WITH_ERR(name, cfunc) extern void name(int);
#define DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(name, cfunc) extern void name(void);

#else /* __ASSEMBLER__ */

#define DECLARE_IRQ_ENTRY_WITH_EOI(name, cfunc) \
    asm_irq_handler_with_eoi name, cfunc

#define DECLARE_IRQ_ENTRY_WITHOUT_EOI(name, cfunc) \
    asm_irq_handler_without_eoi name, cfunc

#define DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(name, cfunc) \
    asm_exception_handler_without_err name, cfunc

#endif /* __ASSEMBLER__ */

/**
 * In the assembly file, generate the asm trampoline.
 * For C files, declare references to the assembly handlers
 */

DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(asm_div_exception_handler, __div_exception_handler)

DECLARE_IRQ_ENTRY_WITH_EOI(asm_irq_apic_oneshot_timer_handler, __apic_oneshot_timer_handler)

#endif /* _HYVEMIND_X64_ASM_IDT_H */

