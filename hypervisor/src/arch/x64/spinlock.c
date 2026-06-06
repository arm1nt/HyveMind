#include "asm/spinlock.h"

static inline bool
atomic_acquire(volatile uint8_t *flag)
{
    uint8_t res;
    uint8_t queried_state = SPINLOCK_FREE;

    asm volatile (
            "lock cmpxchg %3, %1\n\t"
            "sete %0"
            : "=rm"(res), "+m"(*flag), "+a"(queried_state)
            : "r"(SPINLOCK_USED)
    );

    return res;
}

static inline void
atomic_release(volatile uint8_t *flag)
{
    uint8_t release = SPINLOCK_FREE;

    /* Since we use lock semantics this is instruction is serializing */
    asm volatile(
            "lock xchg %1, %0"
            :  "+m"(*flag)
            : "r"(release)
    );
}

/**
 * TODO: Add debug check that requestor processor is allowed to release the
 * lock (i.e. currently owns it)
 */

void
arch_spinlock_acquire(spinlock_t *lock)
{
    while (!atomic_acquire(&lock->flag)) {
        asm volatile ("pause");
    }
}

void
arch_spinlock_release(spinlock_t *lock)
{
    atomic_release(&lock->flag);
}

