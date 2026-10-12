#ifndef _HYVEMIND_X64_ASM_IDT_H
#define _HYVEMIND_X64_ASM_IDT_H

#include "asm/irq_vectors.h"

#define COMMON_ASM_IRQ_TRAMPOLINE_NAME(vec) asm_common_irq_trampoline_ ## vec

#ifndef __ASSEMBLER__

#include "asm/irq.h"

#define DECLARE_IRQ_ENTRY_WITH_EOI(name, cfunc) extern void name(void);
#define DECLARE_IRQ_ENTRY_WITHOUT_EOI(name, cfunc) extern void name(void);
#define DECLARE_EXCEPTTION_HANDLER_WITH_ERR(name, cfunc) extern void name(int);
#define DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(name, cfunc) extern void name(void);

extern void* legacy_irq_asm_trampolines[IRQ_LEGACY_VECTOR_NR];

extern void common_legacy_irq_trampoline(const irq_t irq_vector);
extern void common_legacy_irq_trampoline_spurious(const irq_t irq_vector);

#else /* __ASSEMBLER__ */

#define DECLARE_IRQ_ENTRY_WITH_EOI(name, cfunc) \
    asm_irq_handler_with_eoi name, cfunc

#define DECLARE_IRQ_ENTRY_WITHOUT_EOI(name, cfunc) \
    asm_irq_handler_without_eoi name, cfunc

#define DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(name, cfunc) \
    asm_exception_handler_without_err name, cfunc

#define DEFINE_COMMON_IRQ_TRAMPOLINE_VEC(irq_vec, cfunc)    \
    asm_common_irq_trampoline_vec %(irq_vec), cfunc

#define DEFINE_LEGACY_IRQ_TRAMPOLINE(irq_vec)               \
    DEFINE_COMMON_IRQ_TRAMPOLINE_VEC(irq_vec, common_legacy_irq_trampoline)

#define DEFINE_LEGACY_IRQ_TRAMPOLINE_SPURIOUS(irq_vec)      \
    DEFINE_COMMON_IRQ_TRAMPOLINE_VEC(irq_vec, common_legacy_irq_trampoline_spurious)

#endif /* __ASSEMBLER__ */

/**
 * In the assembly file, generate the asm trampoline.
 * For C files, declare references to the assembly handlers
 */

DECLARE_EXCEPTION_HANDLER_WITHOUT_ERR(asm_div_exception_handler, __div_exception_handler)

DECLARE_IRQ_ENTRY_WITH_EOI(asm_irq_apic_timer_handler, irq_timer_handler)
DECLARE_IRQ_ENTRY_WITH_EOI(asm_irq_apic_schedule_ipi_handler, __apic_schedule_ipi_handler)

#endif /* _HYVEMIND_X64_ASM_IDT_H */

