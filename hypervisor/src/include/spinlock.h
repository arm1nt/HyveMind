#ifndef _HYVEMIND_SPINLOCK_H
#define _HYVEMIND_SPINLOCK_H

#include "asm/spinlock.h"

#ifdef HYVEMIND_DEBUG_BUILD

#define SPINLOCK_DBG_ACQUIRE(lock, statement) \
    statement; \
    SPINLOCK_DBG_RECORD_ACQUIRED_INFO(lock)

#define SPINLOCK_DBG_RELEASE(lock, statement) \
    SPINLOCK_DBG_RECORD_RELEASE_INFO(lock); \
    statement

#else

#define SPINLOCK_DBG_ACQUIRE(lock, statement) statement
#define SPINLOCK_DBG_RELEASE(lock, statement) statement

#endif /* HYVEMIND_DEBUG_BUILD */

#define spinlock_acquire(lock) \
    SPINLOCK_DBG_ACQUIRE(lock, arch_spinlock_acquire(lock))

#define spinlock_release(lock) \
    SPINLOCK_DBG_RELEASE(lock, arch_spinlock_release(lock))

#define __spinlock_acquire_irq_save(lock, flags)        \
    do {                                                \
        flags = arch_spinlock_acquire_irq_save(lock);   \
    } while (0)

#define spinlock_acquire_irq_save(lock, flags) \
    SPINLOCK_DBG_ACQUIRE(lock, __spinlock_acquire_irq_save(lock, flags))

#define __spinlock_release_irq_restore(lock, flags) \
    arch_spinlock_release_irq_restore(lock, flags)

#define spinlock_release_irq_restore(lock, flags) \
    SPINLOCK_DBG_RELEASE(lock, __spinlock_release_irq_restore(lock, flags))

#endif /* _HYVEMIND_SPINLOCK_H */

