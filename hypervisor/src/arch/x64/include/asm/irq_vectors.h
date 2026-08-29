#ifndef _HYVEMIND_X64_ASM_IRQ_VECTORS_H
#define _HYVEMIND_X64_ASM_IRQ_VECTORS_H

#define IRQ_DIVIDE_ERROR_VECTOR     0x00
#define IRQ_NMI_VECTOR              0x02
#define IRQ_UD_VECTOR               0x06
#define IRQ_DOUBLE_FAULT_VECTOR     0x08
#define IRQ_GP_VECTOR               0x0d
#define IRQ_PAGE_FAULT_VECTOR       0x0e
#define IRQ_VIRT_VECTOR             0x14

#define IRQ_APIC_SPURIOUS_VECTOR    0xff
#define IRQ_APIC_TIMER_VECTOR       0x30

#define IRQ_SCHED_IPI_VECTOR        0x50

#endif /*  _HYVEMIND_X64_ASM_IRQ_VECTORS_H */

