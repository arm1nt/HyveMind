#ifndef _HYVEMIND_X64_ASM_SPINLOCK_H
#define _HYVEMIND_X64_ASM_SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include "asm/processor.h"

#define SPINLOCK_FREE   0
#define SPINLOCK_USED   1

#ifdef HYVEMIND_DEBUG_BUILD

struct spinlock_op_position {
    char *func;
    char *file;
    int line;
};

#define SPINLOCK_DBG_RECORD_ACQUIRED_INFO(lock) do {    \
        (lock)->last_acquired.func = __func__;          \
        (lock)->last_acquired.file = __FILE_NAME__;     \
        (lock)->last_acquired.line = __LINE__;          \
    } while (0)

#define SPINLOCK_DBG_RECORD_RELEASE_INFO(lock) do {         \
        (lock)->last_last_released = (lock)->last_released; \
        (lock)->last_released.func = __func__;              \
        (lock)->last_released.file = __FILE_NAME__;         \
        (lock)->last_released.line = __LINE__;              \
    } while (0)

#endif /* HYVEMIND_DEBUG_BUILD */

struct spinlock {
    char *name;
    uint8_t flag;
    uint32_t cpuid;
#ifdef HYVEMIND_DEBUG_BUILD
    struct spinlock_op_position last_acquired;
    struct spinlock_op_position last_last_released;
    struct spinlock_op_position last_released;
#endif
};
typedef struct spinlock spinlock_t;

#define spinlock_init(lock)             \
    do {                                \
        (lock)->flag = SPINLOCK_FREE;   \
    } while (0);

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

