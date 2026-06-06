#ifndef _HYVEMIND_SPINLOCK_H
#define _HYVEMIND_SPINLOCK_H

#include "asm/spinlock.h"

static inline void
spinlock_acquire(spinlock_t *lock)
{
    arch_spinlock_acquire(lock);
}

static inline void
spinlock_release(spinlock_t *lock)
{
    arch_spinlock_release(lock);
}

#endif /* _HYVEMIND_SPINLOCK_H */

