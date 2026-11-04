#include "cpu_topology.h"
#include "fatal.h"
#include "per-cpu.h"
#include "sched.h"
#include "string.h"
#include "vm.h"
#include "asm/vmm.h"

DEFINE_PER_CPU(struct task_queue, task_queue);
#define current_tq ((struct task_queue *) percpu_ptr(task_queue))

static inline void
__init_task_queue(struct task_queue *tq)
{
    memset(tq, 0, sizeof(struct task_queue));
    spinlock_init(&tq->lock);
    INIT_LIST_HEAD(&tq->runnable_tasks);
    INIT_LIST_HEAD(&tq->blocked_tasks);
}

void
init_task_queues(void)
{
    cpuid_t cpuid;
    for_each_cpuid_in_cpumask_present(cpuid) {
        struct task_queue *tq = percpu_ptr_raw(task_queue, cpuid);
        __init_task_queue(tq);
    }
}

static void
do_idle(struct sched_unit *schedu)
{
    cpu_halt_with_irq_window();
}

static struct sched_unit idle_task = {
    .ops.launch_task = do_idle
};

static inline struct sched_unit *
tq_get_next_task(struct task_queue *tq)
{
    struct sched_unit *next;
    spinlock_acquire(&tq->lock);

    next = list_first_entry_nullable(&tq->runnable_tasks, struct sched_unit, sched_queue);
    if (!next)
        goto out;

    list_remove_head(&tq->runnable_tasks);
out:
    spinlock_release(&tq->lock);
    return next;
}

static void
__schedule(void)
{
    disable_local_interrupts();
    spinlock_acquire(&current_cpu->lock);

    struct sched_unit *task = tq_get_next_task(current_tq);
    if (!task) {
        current_cpu->state = CPU_IDLE;
        task = &idle_task;
    }

    spinlock_release(&current_cpu->lock);
    /* Launches with interrupts disabled */
    task->ops.launch_task(task);

    disable_local_interrupts();
    spinlock_acquire(&current_cpu->lock);
    current_cpu->state = CPU_INIT;
    spinlock_release(&current_cpu->lock);
    enable_local_interrupts();
}

static void __attribute__((noreturn))
schedule_loop(void)
{
    while (1)
        __schedule();

    die_reason("unreachable");
}

void __attribute__((noreturn))
schedule(void)
{
    /**
     * Reset stack to scheduler stack and jump into the schedule loop.
     *  - we don't save any information about the caller or modify
     *  a task queue here.
     */
    NOT_YET_IMPLEMENTED;
}

void __attribute__((noreturn))
enter_schedule_loop(void)
{
    /* Todo: Allocate a clean scheduler stack and switch to it */
    schedule_loop();
}

static inline void
tq_submit_runnable_task(struct task_queue *tq, struct sched_unit *schedu)
{
    uint64_t flags;
    spinlock_acquire_irq_save(&tq->lock, flags);
    list_add_tail(&tq->runnable_tasks, &schedu->sched_queue);
    spinlock_release_irq_restore(&tq->lock, flags);
}

/**
 * 'sched_submit_vm' will only be called by the bsp, so we don't need
 * any synchronization for this counter
 */
static cpuid_t prev_submission_tq_cpuid = INVALID_CPUID;

static inline cpuid_t
get_next_submission_tq_cpuid(void)
{
    if (prev_submission_tq_cpuid == INVALID_CPUID) {
        prev_submission_tq_cpuid = get_next_cpuid_in_mask(cpumask, 0);
    } else {
        prev_submission_tq_cpuid = get_next_cpuid_in_mask(cpumask, prev_submission_tq_cpuid + 1);
        if (prev_submission_tq_cpuid == INVALID_CPUID) {
            return get_next_submission_tq_cpuid();
        }
    }
    return prev_submission_tq_cpuid;
}

void
sched_submit_vm(struct vm *vm)
{
    int vcpu_idx = 0;
    uint64_t flags;
    cpuid_t cpuid;
    cpu_t *target_cpu;

    while (vcpu_idx < vm->nr_vcpus) {
        cpuid = get_next_submission_tq_cpuid();
        target_cpu = percpu_ptr_raw(cpu, cpuid);

        struct task_queue *tq = percpu_ptr_raw(task_queue, cpuid);
        tq_submit_runnable_task(tq, &vm->vcpus[vcpu_idx]->sched_unit);

        spinlock_acquire_irq_save(&target_cpu->lock, flags);
        if (target_cpu->state == CPU_IDLE)
            /* Wake up an idling cpu so that it attempts to schedule a new task */
            kick_cpu(target_cpu->id);
        spinlock_release_irq_restore(&target_cpu->lock, flags);

        vcpu_idx++;
    }
}

