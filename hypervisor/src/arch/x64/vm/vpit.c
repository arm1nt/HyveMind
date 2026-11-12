#include "fatal.h"
#include "halloc.h"
#include "mm_types.h"
#include "string.h"
#include "timer.h"
#include "vm/irq.h"
#include "vm/vpit.h"
#include "vm/vpit_defs.h"

/**
 * Minimal implementation that supports booting the linux kernel.
 * We only support counter0+mode2 and we don't support read operations.
 */

static const bool supported_counters[] = {
    [VPIT_COUNTER0] = true,
    false,
};

static const bool supported_counter_modes[] = {
    [VPIT_COUNTER_MODE2] = true,
    false
};

struct vpit *
allocate_vpit(struct vm *vm)
{
    struct vpit *vpit = (struct vpit *) hmalloc(sizeof(struct vpit));
    if (!vpit)
        return NULL;

    memset(vpit, 0, sizeof(struct vpit));
    vpit->vm = vm;
    vpit->counters[0].id = VPIT_COUNTER0;
    vpit->counters[1].id = VPIT_COUNTER1;
    vpit->counters[2].id = VPIT_COUNTER2;
    return vpit;
}

void
destroy_vpit(struct vpit *vpit)
{
    struct vpit_counter *counter;

    for (int i = 0; i < VPIT_COUNTERS_NR; i++) {
        counter = &vpit->counters[i];
        if (counter->timer)
            delete_timer(counter->timer);
    }

    hfree(vpit);
}

static inline struct vpit *
vpit_of_counter(struct vpit_counter *counter)
{
    const uint64_t offset = offsetof(struct vpit, counters[counter->id]);
    const virt_addr_t vpit_ptr = ((virt_addr_t) counter) - offset;
    return (struct vpit *) vpit_ptr;
}

static inline enum vpit_counter_rw_mode
cwr_get_rw_mode(const uint8_t cwr)
{
    const uint8_t raw_rw_val = (cwr >> CWR_RW_VAL_SHIFT) & CWR_RW_VAL_MASK;

    switch (raw_rw_val) {
        case CWR_RW_LSB:
            return VPIT_RW_LSB;
        case CWR_RW_MSB:
            return VPIT_RW_MSB;
        case CWR_RW_LSB_MSB:
            return VPIT_RW_LSB_MSB;
    }
    unreachable();
}

static inline enum vpit_counter_mode
cwr_get_mode(const uint8_t cwr)
{
    const uint8_t raw_mode_val = (cwr >> CWR_MODE_VAL_SHIFT) & CWR_MODE_VAL_MASK;

    switch (raw_mode_val) {
        case CWR_MODE0_VAL:
            return VPIT_COUNTER_MODE0;
        case CWR_MODE1_VAL:
            return VPIT_COUNTER_MODE1;
        case CWR_MODE2_VAL1:
        case CWR_MODE2_VAL2:
            return VPIT_COUNTER_MODE2;
        case CWR_MODE3_VAL1:
        case CWR_MODE3_VAL2:
            return VPIT_COUNTER_MODE3;
        case CWR_MODE4_VAL:
            return VPIT_COUNTER_MODE4;
        case CWR_MODE5_VAL:
            return VPIT_COUNTER_MODE5;
    }
    unreachable();
}

static void
cwr_configure_counter(struct vpit_counter *counter, const uint8_t cwr)
{
    const enum vpit_counter_mode mode = cwr_get_mode(cwr);
    if (!supported_counter_modes[mode]) {
        pr_error("Attempted to set unsupported vPIT counter mode %lu", mode);
        die();
    }
    counter->mode = mode;

    if (cwr & CWR_BCD_VAL) {
        pr_error("vPIT does not support binary coded decimal counters");
        die();
    }

    counter->rw.mode = cwr_get_rw_mode(cwr);
    counter->rw.write_wait_msb = false;
    counter->count_register.full = 0;
    if (counter->timer)
        delete_timer(counter->timer);
}

static void
handle_cwr_write(struct vpit *vpit, const uint8_t cwr)
{
    if (IS_READ_BACK_CMD(cwr)) {
        pr_error("vPIT does not suport the 'read_back' cmd");
        die();
    } else if (IS_COUNTER_LATCH_CMD(cwr)) {
        pr_error("vPIT does not support the 'counter_latch' cmd");
        die();
    }

    const enum vpit_counter_id counter_id = cwr >> CWR_SC_VAL_SHIFT;
    if (!supported_counters[counter_id])
        return;

    cwr_configure_counter(&vpit->counters[counter_id], cwr);
}

static inline uint64_t
sanitize_counter_period(const uint16_t ticks)
{
    const uint64_t target_period_ns = (ticks * sec_to_ns(1)) / VPIT_FREQUENCY;
    const uint64_t sanitized_period = MAX(target_period_ns, VPIT_MIN_PERIOD);
    pr_debug("Target period: %lu ns\nSanitized period: %lu ns", target_period_ns, sanitized_period);
    return sanitized_period;
}

static void
mode2_counter_fn(uint64_t tsc_now, void *data)
{
    struct vpit_counter *counter = (struct vpit_counter *) data;
    struct vpit *vpit = vpit_of_counter(counter);
    vm_inject_external_guest_irq(VPIT_IRQ_NR, vpit->vm);
}

static inline void
handle_mode2_counter_write(struct vpit_counter *counter)
{
    int ret;
    struct timer *timer;

    if (counter->rw.write_wait_msb)
        return;

    /**
     * Here we deviate from the i8254 specification. We don't wait until the
     * current timer fires and then afterwards re-arm it with the updated counter
     * value, but instead immediately delete the potentially currently ticking
     * timer and install a new one with the newly written counter value.
     */
    if (counter->timer)
        delete_timer(counter->timer);

    const uint64_t period_ns = sanitize_counter_period(counter->count_register.full);

    timer = init_timer_ns(period_ns, TIMER_PERIODIC);
    timer->data = counter;
    timer->timer_action = mode2_counter_fn;
    counter->timer = timer;

    if ((ret = add_timer(timer)) != TIMER_SUCCESS) {
        pr_error("Failed to add timer for mode2-counter: %lu", ret);
        die();
    }
}

static inline void
__write_count_register(struct vpit_counter *counter, const uint8_t val)
{
    const enum vpit_counter_rw_mode mode = counter->rw.mode;

    switch (mode) {
        case VPIT_RW_LSB:
            counter->count_register.full = U16(val);
            break;
        case VPIT_RW_MSB:
            counter->count_register.full = U16(val) << 8;
            break;
        case VPIT_RW_LSB_MSB:
            if (!counter->rw.write_wait_msb) {
                counter->count_register.lsb = val;
                counter->rw.write_wait_msb = true;
            } else {
                counter->count_register.msb = val;
                counter->rw.write_wait_msb = false;
            }
            break;
    }
}

static void
handle_counter_write(struct vpit_counter *counter, const uint8_t val)
{
    __write_count_register(counter, val);

    switch (counter->mode) {
        case VPIT_COUNTER_MODE2:
            handle_mode2_counter_write(counter);
            break;
        default:
            unreachable();
    }
}

void
vpit_write(struct vpit *vpit, const uint16_t port, const uint8_t val)
{
    const uint8_t port_selector = port & PORT_SELECTOR_MASK;

    if (port_selector == CWR_PORT) {
        handle_cwr_write(vpit, val);
        return;
    }

    const enum vpit_counter_id id = port_selector;
    if (!supported_counters[id])
        return;

    handle_counter_write(&vpit->counters[id], val);
}

void
vpit_read(struct vpit *vpit, const uint16_t port, uint8_t *res)
{
    const uint8_t port_selector = port & PORT_SELECTOR_MASK;
    if (port_selector == CWR_PORT)
        /* no-op */
        return;

    const enum vpit_counter_id id = port_selector;
    if (!supported_counters[id])
        return;

    pr_error("vPIT does not support reads");
    die();
}

