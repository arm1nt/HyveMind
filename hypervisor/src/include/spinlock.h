#ifndef _HYVEMIND_SPINLOCK_H
#define _HYVEMIND_SPINLOCK_H

#include "asm/spinlock.h"

#define spinlock_acquire(lock) arch_spinlock_acquire(lock)
#define spinlock_release(lock) arch_spinlock_release(lock)

#define spinlock_acquire_irq_save(lock, flags)          \
    do {                                                \
        flags = arch_spinlock_acquire_irq_save(lock);   \
    } while (0)

#define spinlock_release_irq_restore(lock, flags)       \
    arch_spinlock_release_irq_restore(lock, flags)

#endif /* _HYVEMIND_SPINLOCK_H */

