#include "halloc.h"
#include "per-cpu.h"
#include "printf.h"
#include "timer.h"
#include "asm/timer.h"
#include "asm/processor.h"

/* Maintain a min-heap to order how we program the timer */

struct timer_heap;

DEFINE_PER_CPU(struct timer_heap *, timer_heap);

#define HEAP_ROOT_INDEX         1
#define heap_parent_idx(idx)    ((idx) / 2)
#define heap_left_idx(idx)      ((idx) * 2)
#define heap_right_idx(idx)     (((idx) * 2) + 1)
#define heap_max_idx(heap)      ((heap)->insertion_idx - 1)

#define INIT_TIMER_HEAP_CAPACITY 512

struct timer_heap {
    struct timer **flattened_tree;
    unsigned int capacity;
    unsigned int insertion_idx;
};

static int
init_timer_heap(void)
{
    struct timer_heap *heap = (struct timer_heap *) hmalloc(sizeof(struct timer_heap));
    if (!heap) {
        pr_error("Failed to allocate timer heap");
        return TIMER_NO_MEM;
    }

    heap->flattened_tree =
        (struct timer **) hmalloc(INIT_TIMER_HEAP_CAPACITY * sizeof(struct timer*));
    if (!heap->flattened_tree) {
        pr_error("Failed to allocate the internal heap array");
        hfree(heap);
        return TIMER_NO_MEM;
    }

    heap->capacity = INIT_TIMER_HEAP_CAPACITY;
    heap->insertion_idx = HEAP_ROOT_INDEX;

    set_percpu_val(timer_heap, heap);

    return TIMER_SUCCESS;
}

static inline void
heap_swap(struct timer **arr, const unsigned int idx1, const unsigned int idx2)
{
    struct timer *temp = arr[idx1];
    arr[idx1] =  arr[idx2];
    arr[idx2] = temp;
}

static void
heapify_up(struct timer_heap *heap, const unsigned int node_idx)
{
    unsigned int idx = node_idx;
    unsigned int parent_idx;

    while ((parent_idx = heap_parent_idx(idx)) > 0) {
        const uint64_t node_val =  heap->flattened_tree[idx]->elapse_deadline;
        const uint64_t parent_val = heap->flattened_tree[parent_idx]->elapse_deadline;

        if (parent_val <= node_val) {
            break;
        }

        heap_swap(heap->flattened_tree, idx, parent_idx);
        idx = parent_idx;
    }
}

static void
heapify_down(struct timer_heap *heap, const unsigned int node_idx)
{
    unsigned int largest, left, right;
    unsigned int idx = node_idx;
    const unsigned int max_idx = heap_max_idx(heap);
    struct timer *left_val, *right_val, *largest_val;

    while (true) {
        largest = idx;
        left = heap_left_idx(idx);
        right = heap_right_idx(idx);
        left_val = heap->flattened_tree[left];
        right_val = heap->flattened_tree[right];
        largest_val = heap->flattened_tree[largest];


        if ((left <= max_idx) &&
                (left_val->elapse_deadline < largest_val->elapse_deadline)) {
            largest = left;
            largest_val = left_val;
        }

        if ((right <= max_idx)
                && (right_val->elapse_deadline < largest_val->elapse_deadline)) {
            largest = right;
        }

        if (largest == idx) {
            break;
        }

        heap_swap(heap->flattened_tree, idx, largest);
        idx = largest;
    }
}

static void
delete_heap_element(struct timer_heap *heap, const unsigned int idx)
{
    if (heap->insertion_idx <= idx) {
        pr_warn("Node with index %lu does not exist", idx);
        return;
    }

    heap->flattened_tree[idx] = heap->flattened_tree[heap_max_idx(heap)];
    heap->insertion_idx--;
    heapify_down(heap, idx);
}

static inline void
delete_root(struct timer_heap *heap)
{
    delete_heap_element(heap, HEAP_ROOT_INDEX);
}

static inline bool
get_heap_root(struct timer_heap *heap, struct timer** value)
{
    if (heap->insertion_idx <= HEAP_ROOT_INDEX) {
        pr_debug("Cannot get root of empty heap");
        *value =  NULL;
        return false;
    }

    *value = heap->flattened_tree[HEAP_ROOT_INDEX];
    return true;
}

static int
heap_add_timer(struct timer_heap *heap, struct timer *timer)
{
    if (heap->insertion_idx >= heap->capacity) {
        /* Maybe make growable in the future. */
        pr_error("Max timer capacity reached. Cannot add timer...");
        return TIMER_NO_CAPACITY;
    }

    heap->flattened_tree[heap->insertion_idx] = timer;
    heapify_up(heap, heap->insertion_idx);
    heap->insertion_idx++;

    return TIMER_SUCCESS;
}

struct deadline_info {
    uint64_t ticks;
    uint64_t deadline;
};

static inline uint64_t
__get_ticks_from_ns(const uint64_t time_ns)
{
    const uint64_t clock_hz = arch_get_timer_frequency();
    const uint64_t req_ticks =
        ((U128(clock_hz) * time_ns) + (sec_to_ns(1) - 1)) / U64(sec_to_ns(1));

    return req_ticks;
}

static inline uint64_t
__get_ticks_from_hz(const uint64_t ticks, const uint64_t hz)
{
    const uint64_t clock_hz = arch_get_timer_frequency();
    const uint64_t req_ticks = (U128(clock_hz) * ticks) / hz;
    return req_ticks;
}

struct timer *
__init_timer(const uint64_t deadline, const uint64_t ticks, const enum timer_type type)
{
    struct timer *timer = (struct timer *) hmalloc(sizeof(struct timer));
    if (!timer) {
        pr_error("Failed to allocate timer struct");
        return NULL;
    }

    timer->elapse_deadline = deadline;
    timer->periodic_ticks = ticks;
    timer->type = type;
    timer->cpu = get_current_cpuid();
    timer->timer_action = NULL;
    timer->data = NULL;

    return timer;
}

struct timer *
init_timer_deadline(const uint64_t timer_deadline)
{
    return __init_timer(timer_deadline, 0, TIMER_ONESHOT);
}

struct timer *
init_timer_ns(const uint64_t time_ns, const enum timer_type type)
{
    const uint64_t now = read_tsc();
    const uint64_t ticks = __get_ticks_from_ns(time_ns);
    return __init_timer(now + ticks, ticks, type);
}

struct timer *
init_timer_hz(const uint64_t hz, const uint64_t ticks, const enum timer_type type)
{
    const uint64_t now = read_tsc();
    const   uint64_t timer_ticks = __get_ticks_from_hz(ticks, hz);
    return __init_timer(now + timer_ticks, timer_ticks, type);
}

int
add_timer(struct timer *timer)
{
    const uint64_t status = disable_irq_save_status();

    int ret;
    struct timer_heap *heap = percpu_val(timer_heap);
    struct timer *earliest_deadline;

    if (heap_add_timer(heap, timer) != TIMER_SUCCESS) {
        pr_error("Failed to add timer to the timer heap");
        restore_irq_status(status);
        return TIMER_ERROR;
    }

    get_heap_root(heap, &earliest_deadline);
    if (earliest_deadline->elapse_deadline == timer->elapse_deadline) {

        ret = arch_reprogram_timer(timer->elapse_deadline);
        if (ret != TIMER_SUCCESS) {
            pr_error("Failed to reprogram time to new deadline %lu", timer->elapse_deadline);
            restore_irq_status(status);
            return TIMER_PROGRAMMING_FAILED;
        }
    }

    restore_irq_status(status);
    return TIMER_SUCCESS;
}

void
irq_timer_handler(void)
{
    const uint64_t now = read_tsc();
    struct timer_heap *heap = percpu_val(timer_heap);
    struct timer *earliest_timer;

    while (1) {
        if (!get_heap_root(heap, &earliest_timer)) {
            /* No timer present to even re-arm the hw timer */
            return;
        }

        if (earliest_timer->elapse_deadline > read_tsc()) {
            arch_reprogram_timer(earliest_timer->elapse_deadline);
            break;
        }

        earliest_timer->timer_action(now, earliest_timer->data);
        delete_root(heap);

        if (earliest_timer->type == TIMER_PERIODIC) {
            earliest_timer->elapse_deadline += earliest_timer->periodic_ticks;
            add_timer(earliest_timer);
        }
    }
}


int
init_timer_framework(void)
{
    int ret;

    if ((ret = arch_init_timer_framework()) != TIMER_SUCCESS) {
        return ret;
    }

    if ((ret = init_timer_heap()) != TIMER_SUCCESS) {
        pr_error("Error initializing timer heap");
        return ret;
    }

    return TIMER_SUCCESS;
}

