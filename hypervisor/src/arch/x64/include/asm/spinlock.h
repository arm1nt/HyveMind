#ifndef _HYVEMIND_X64_ASM_SPINLOCK_H
#define _HYVEMIND_X64_ASM_SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define SPINLOCK_FREE   0
#define SPINLOCK_USED   1

struct spinlock {
    char *name;
    uint8_t flag;
    uint32_t cpuid;
};
typedef struct spinlock spinlock_t;

#define DEFINE_SPINLOCK(_name)              \
    static spinlock_t _name = {             \
        .flag = SPINLOCK_FREE,              \
        .name = #_name                      \
    }

void arch_spinlock_acquire(spinlock_t *lock);

void arch_spinlock_release(spinlock_t *lock);

#endif /* _HYVEMIND_X64_ASM_SPINLOCK_H */

