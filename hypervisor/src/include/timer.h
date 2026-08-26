#ifndef _HYVEMIND_TIMER_H
#define _HYVEMIND_TIMER_H

#include "types.h"

#define sec_to_ns(sec) ((sec) * 1000000000)
#define ms_to_ns(req_ns) ((req_ns) * 1000000)

enum timer_type {
    ONE_SHOT_TIMER,
    PERIODIC_TIMER,
};

struct timer {
    uint64_t tsc_deadline;
    void (*fn) (void *);
    /* cpu on which the timer is installed */
    unsigned int cpu;

};

extern void arch_do_busy_sleep(const uint64_t time_ns);

static inline void
do_busy_sleep(const uint64_t time_ns)
{
    arch_do_busy_sleep(time_ns);
}

#endif /* _HYVEMIND_TIMER_H */

