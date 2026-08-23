/**
 * Very very stripped down implementation. We only use the HPET counter as a
 * fallback to compute the TSC frequency if necessary, for nothing else.
 */
#include "printf.h"
#include "asm/timer.h"
#include "asm/pgtables.h"
#include "asm/processor.h"
#include "drivers/hpet.h"

/**
 * Qemu usually maps the HPET registers to this address. Until we implement ACPI
 * support, we'll require/except a HPET to be at this location.
 */
#define HPET_BASE U64(0xFED00000)

#define HPET_CAP_ID_REG                 0x00
#define HPET_GENERAL_CONFIG_REG         0x10
#define HPET_GENERAL_IRQ_STATUS_REG     0x20
#define HPET_MAIN_COUNTER_REG           0xF0

#define HPET_CAPS_REV_ID_MASK           0xFF
#define HPET_CAPS_COUNT_SIZE_SHIFT      0x0D
#define HPET_CAPS_COUNT_SIZE_MASK       0x01
#define HPET_CAPS_NR_TIMER_SHIFT        0x08
#define HPET_CAPS_NR_TIMER_MASK         0x1F
#define HPET_ENABLE_COUNTER_MASK        0x01
#define HPET_TIMER_IRQ_ENABLE_MASK      U64(0x02)

#define NTH_TIMER_CONFIG_REG(nr) ((0x20 * (nr)) + 0x100)

#define HPET_MIN_TICK_GRANULARITY_NS    100
#define HPET_MIN_CLOCK_PERIOD           1
#define HPET_MAX_CLOCK_PERIOD           0x05F5E100

static uint64_t hpet_hz = 0;

static inline uint64_t
hpet_readq(const int reg)
{
    const uint64_t reg_mem = HPET_BASE + reg;
    return *((uint64_t *) reg_mem);
}

static inline void
hpet_writeq(const int reg, const uint64_t val)
{
    const uint64_t reg_mem = HPET_BASE + reg;
    uint64_t *ptr = (uint64_t *) reg_mem;
    *ptr = val;
}

#define femtosec_to_sec(fs) ((fs) * 1000000000000000) /* times 10^15 */
#define hz_to_mhz(hz) ((hz) / 1000000)

static inline uint64_t
hpet_period_to_hz(const uint64_t period_fs)
{
    return femtosec_to_sec(1) / period_fs;
}

/* Should be called with interrupts disabled */
void
hpet_do_busy_sleep(const uint64_t time_ns)
{
    const uint64_t start_counter = hpet_readq(HPET_MAIN_COUNTER_REG);
    const uint64_t ticks_per_100ns = hpet_hz / 10000000;
    const uint64_t req_ticks = (time_ns * ticks_per_100ns) / HPET_MIN_TICK_GRANULARITY_NS;
    const uint64_t target_counter = start_counter + req_ticks;

    /* Safe even for overflows since we simply count until the wrapped around value */
    while (hpet_readq(HPET_MAIN_COUNTER_REG) < target_counter) {
        cpu_relax();
    }
}

int
init_hpet(void)
{
    if (identity_map_mmio_page(HPET_BASE) != 0) {
        pr_error("Failed to map the base HPET timer block register region");
        return HPET_ERROR;
    }

    const uint64_t caps = hpet_readq(HPET_CAP_ID_REG);

    if ((caps & HPET_CAPS_REV_ID_MASK) == 0) {
        pr_warn("HPET timer specifies the invalid revision id '0'");
        goto hpet_unusable;
    }

    if (((caps >> HPET_CAPS_COUNT_SIZE_SHIFT) && HPET_CAPS_COUNT_SIZE_MASK) == 0) {
        pr_warn("HPET timer does not support operation in 64-bit mode");
        goto hpet_unusable;
    }

    const int nr_timers =
        ((caps >> HPET_CAPS_NR_TIMER_SHIFT) & HPET_CAPS_NR_TIMER_MASK) + 1;

    /* Ensuring that no timer tries to generate an interrupt */
    for (int i = 0; i < nr_timers; i++) {
        uint64_t timer_config = hpet_readq(NTH_TIMER_CONFIG_REG(i));
        timer_config &= ~HPET_TIMER_IRQ_ENABLE_MASK;
        hpet_writeq(NTH_TIMER_CONFIG_REG(i), timer_config);
    }

    uint64_t timer_config = hpet_readq(HPET_GENERAL_CONFIG_REG);
    timer_config |= HPET_ENABLE_COUNTER_MASK;
    hpet_writeq(HPET_GENERAL_CONFIG_REG, timer_config);

    const uint32_t clock_period = U64_UPPER32(caps);
    if ((clock_period < HPET_MIN_CLOCK_PERIOD) || (clock_period > HPET_MAX_CLOCK_PERIOD)) {
        pr_warn("HPET timer specifies invalid clock period of '0x%lx' fs. "
                "The allowed period range is (0x%lx, 0x%lx).",
                clock_period,
                HPET_MIN_CLOCK_PERIOD,
                HPET_MAX_CLOCK_PERIOD
        );
        goto hpet_unusable;
    }

    hpet_hz = hpet_period_to_hz(clock_period);
    pr_info("The HPET timer is running at a frequency of %lu HZ (%lu MHZ)",
            hpet_hz,
            hz_to_mhz(hpet_hz)
    );

    return HPET_SUCCESS;

hpet_unusable:
    pr_debug("Unable to use HPET as clocksource...");
    hpet_hz = 0;
    return HPET_NOT_SUPPORTED;
}

