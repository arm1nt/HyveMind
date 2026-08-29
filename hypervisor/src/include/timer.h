#ifndef _HYVEMIND_TIMER_H
#define _HYVEMIND_TIMER_H

#include "types.h"
#include "asm/arch_types.h"

#define sec_to_ns(sec) ((sec) * U64(1000000000))
#define ms_to_ns(req_ns) ((req_ns) * U64(1000000))

enum timer_op_status {
    TIMER_SUCCESS,
    TIMER_INIT_FAILED,
    TIMER_NO_MEM,
    TIMER_NO_CAPACITY,
    TIMER_PROGRAMMING_FAILED,
    TIMER_ERROR,
};

enum timer_type {
    TIMER_ONESHOT,
    TIMER_PERIODIC,
};

struct timer {
    uint64_t elapse_deadline;
    uint64_t periodic_ticks;
    enum timer_type type;
    void (*timer_action) (uint64_t tsc_irq_invoke, void *data);
    void *data;
    /* cpu on which the timer is installed */
    cpuid_t cpu;
};

extern void arch_do_busy_sleep(const uint64_t time_ns);

static inline void
do_busy_sleep(const uint64_t time_ns)
{
    arch_do_busy_sleep(time_ns);
}

int init_timer_framework(void);

#endif /* _HYVEMIND_TIMER_H */

