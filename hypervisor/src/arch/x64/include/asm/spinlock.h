#ifndef _HYVEMIND_X64_ASM_SPINLOCK_H
#define _HYVEMIND_X64_ASM_SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include "asm/processor.h"

#define SPINLOCK_FREE   0
#define SPINLOCK_USED   1

struct spinlock {
    char *name;
    uint8_t flag;
    uint32_t cpuid;
};
typedef struct spinlock spinlock_t;

#define DECLARE_SPINLOCK_NAMED(_name)   \
    {                                   \
        .flag = SPINLOCK_FREE,          \
        .name = #_name,                 \
    }

#define DECLARE_SPINLOCK_RAW() DECLARE_SPINLOCK_NAMED()

#define DEFINE_SPINLOCK(_name)      \
    static spinlock_t _name = DECLARE_SPINLOCK_NAMED(_name)

void arch_spinlock_acquire(spinlock_t *lock);
void arch_spinlock_release(spinlock_t *lock);
uint64_t arch_spinlock_acquire_irq_save(spinlock_t *lock);

#define arch_spinlock_release_irq_restore(lock, flags)  \
    do {                                                \
        arch_spinlock_release(lock);                    \
        restore_irq_status(flags);                      \
    } while (0)

#endif /* _HYVEMIND_X64_ASM_SPINLOCK_H */

