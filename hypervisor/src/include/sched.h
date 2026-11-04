#ifndef _HYVEMIND_SCHED_H
#define _HYVEMIND_SCHED_H

#include "spinlock.h"
#include "lib/list.h"

struct vm;

struct sched_unit {
    struct {
        void (*launch_task)(struct sched_unit *schedu);
    } ops;
    struct list_head sched_queue;
};

struct task_queue {
    spinlock_t lock;
    struct list_head runnable_tasks;
    struct list_head blocked_tasks;
};

void init_task_queues(void);

void __attribute__((noreturn)) enter_schedule_loop(void);
void __attribute__((noreturn)) schedule(void);

void sched_submit_vm(struct vm *vm);

#endif /* _HYVEMIND_SCHED_H */

