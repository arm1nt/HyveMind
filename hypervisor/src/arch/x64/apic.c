#include "fatal.h"
#include "mm_types.h"
#include "per-cpu.h"
#include "timer.h"
#include "asm/apic.h"
#include "asm/apic_defs.h"
#include "asm/irq_vectors.h"
#include "asm/pgtables.h"

DEFINE_PER_CPU(uint64_t, apic_hz);
DEFINE_PER_CPU_VAL(bool, tsc_deadline_timer_configured, false);

static inline uint64_t
__x2apic_msr_read(const unsigned int reg)
{
    const uint64_t msr = APIC_MSR_BASE + (reg >> 4);
    return read_msr(msr);
}

static inline void
__x2apic_msr_write(const unsigned int reg, const uint32_t val)
{
    const uint64_t msr = APIC_MSR_BASE + (reg >> 4);
    write_msr(msr, val);
}

static inline uint64_t
__xapic_mem_read(const unsigned int reg)
{
    const virt_addr_t reg_mem = APIC_MEM_BASE + reg;
    return *((uint32_t *) reg_mem);
}

static inline void
__xapic_mem_write(const unsigned int reg, const uint32_t val)
{
    const virt_addr_t reg_mem = APIC_MEM_BASE + reg;
    uint32_t *ptr = (uint32_t *) reg_mem;
    *ptr = val;
}

static inline void
apic_write(const unsigned int reg, const uint32_t val)
{
    if (x2apic_mode_enabled()) {
        __x2apic_msr_write(reg, val);
    } else {
        __xapic_mem_write(reg, val);
    }
}

static inline uint64_t
apic_read(const unsigned int reg)
{
    if (x2apic_mode_enabled()) {
        return __x2apic_msr_read(reg);
    } else {
        return __xapic_mem_read(reg);
    }
}

#define __mask_lvt_entry(entry) SET_BIT(entry, APIC_LVT_MASK_BIT)
#define __unmask_lvt_entry(entry) CLEAR_BIT(entry, APIC_LVT_MASK_BIT)

static inline void
mask_lvt_entry(const int reg)
{
    uint32_t lvt_entry = apic_read(reg);
    lvt_entry = __mask_lvt_entry(lvt_entry);
    apic_write(reg, lvt_entry);
}

#define __set_lvt_vector(entry, vector) (((entry) & ~0xFF) | (vector))

static inline void
set_lvt_vector(const int reg, const uint8_t vector)
{
    uint32_t lvt_entry = apic_read(reg);
    lvt_entry = __set_lvt_vector(lvt_entry, vector);
    apic_write(reg, lvt_entry);
}

static inline void
set_divide_config_register(const enum apic_frequency_divisor freq_div)
{
    const uint32_t val = (freq_div >= 4) ? freq_div+4 : freq_div;
    apic_write(APIC_DIV_CONFIG_REG, val);
}

static inline void
configure_timer_lvt(const enum apic_timer_type timer_type)
{
    uint32_t lvt_entry = apic_read(APIC_LVT_TIMER_REG);

    lvt_entry = __set_lvt_vector(lvt_entry, IRQ_APIC_TIMER_VECTOR);

    uint32_t timer_mode_clear_mask = ~U32_LSHIFT(0x3, APIC_LVT_TIMER_MODE_POS);
    lvt_entry &= timer_mode_clear_mask;
    lvt_entry |= U32_LSHIFT(timer_type, APIC_LVT_TIMER_MODE_POS);

    lvt_entry = __unmask_lvt_entry(lvt_entry);

    apic_write(APIC_LVT_TIMER_REG, lvt_entry);
}

int
apic_program_tsc_deadline_timer(const uint64_t tsc_deadline)
{
    if (!percpu_val(tsc_deadline_timer_configured)) {
        configure_timer_lvt(APIC_TSC_DEADLINE_TIMER);
        set_percpu_val(tsc_deadline_timer_configured, true);
    }

    write_msr(APIC_TSC_DEADLINE_MSR, tsc_deadline);
    return APIC_SUCCESS;
}

int
apic_program_oneshot_timer(
        const uint64_t initial_count,
        const enum apic_frequency_divisor freq_div
) {
    NOT_YET_IMPLEMENTED;
}

static void
determine_apic_timer_frequency(void)
{
    configure_timer_lvt(APIC_ONESHOT_TIMER);
    mask_lvt_entry(APIC_LVT_TIMER_REG);
    set_divide_config_register(APIC_FREQ_DIV_1);

    apic_write(APIC_INITIAL_COUNT_REG, ~U32(0));

    do_busy_sleep(ms_to_ns(100));

    const uint32_t curr_count = apic_read(APIC_CURR_COUNT_REG);
    apic_write(APIC_INITIAL_COUNT_REG, 0);

    const uint64_t delta = U64(~U32(0)) - curr_count;
    set_percpu_val(apic_hz, delta * 10);

    apic_write(APIC_LVT_TIMER_REG, APIC_LVT_RESET_VAL);

    pr_info("apic timer runs at %lu HZ", percpu_val(apic_hz));
}

static inline void
set_task_priority_class(const enum apic_task_priority prio)
{
    write_cr8(prio);
}

static bool
exists_cmci_lvt_entry(void)
{
    const cpuid_result_t res = cpuid_raw(CPUID_CPU_FEATURES_LEAF, NO_SUBLEAF_INDEX);

    if (IS_CLEAR(res.edx, CPUID_MCE)) {
        return false;
    }

    if (IS_CLEAR(res.edx, CPUID_MCA)) {
        return false;
    }

    const uint64_t mcg_cap = read_msr(MSR_IA32_MCG_CAP);

    if (IS_CLEAR(mcg_cap, MSR_MCG_CAP_MCP_CMCI_P)) {
        return false;
    }

    return true;
}

static inline void
reset_lvt_entries(void)
{
    if (exists_cmci_lvt_entry()) {
        apic_write(APIC_LVT_CMCI_REG, APIC_LVT_RESET_VAL);
    }

    apic_write(APIC_LVT_TIMER_REG, APIC_LVT_RESET_VAL);
    apic_write(APIC_LVT_THERMAL_REG, APIC_LVT_RESET_VAL);
    apic_write(APIC_LVT_PERFMON_REG, APIC_LVT_RESET_VAL);
    apic_write(APIC_LVT_LINT0_REG, APIC_LVT_RESET_VAL);
    apic_write(APIC_LVT_LINT1_REG, APIC_LVT_RESET_VAL);
    apic_write(APIC_LVT_ERROR_REG, APIC_LVT_RESET_VAL);
}

static inline void
reset_lapic_state(void)
{
    apic_write(APIC_SVR_REG, APIC_SVR_RESET_VAL);
    reset_lvt_entries();
}

static inline void
sw_enable_lapic(void)
{
    apic_svr_t svr;
    svr.raw = apic_read(APIC_SVR_REG);

    if (svr.sw_enabled) {
        pr_debug("Attempt to enable already enabled lapic");
        return;
    }

    svr.sw_enabled = 1;
    apic_write(APIC_SVR_REG, svr.raw);
}

static int
directly_map_apic_page(void)
{
    const phys_addr_t apic_base = get_lapic_base();
    if (identity_map_mmio_page(apic_base) != 0) {
        pr_error("Unable to directly map apic page for mmio");
        return APIC_ERROR;
    }

    return APIC_SUCCESS;
}

static bool
try_enable_x2apic_mode(void)
{
    uint64_t apic_base_msr;
    const cpuid_result_t res =
        cpuid_raw(CPUID_VERSION_FEATURES_LEAF, NO_SUBLEAF_INDEX);

    if (IS_CLEAR(res.ecx, CPUID_X2APIC_MODE)) {
        pr_info("x2APIC mode not supported. Remaining in xapic mode...");
        return false;
    }

    pr_info("x2APIC mode supported. Switching to x2APIC mode...");

    apic_base_msr = read_msr(MSRX64_IA32_APIC_BASE);
    apic_base_msr |= MSR_APIC_BASE_EXTD;
    write_msr(MSRX64_IA32_APIC_BASE, apic_base_msr);
    return true;
}

static inline bool
is_lapic_present(void)
{
    const cpuid_result_t res =
        cpuid_raw(CPUID_VERSION_FEATURES_LEAF, NO_SUBLEAF_INDEX);

    return IS_SET(res.edx, CPUID_LAPIC);
}

int
setup_bsp_apic(void)
{
    if (!is_lapic_present()) {
        return APIC_NOT_PRESENT;
    }

    if (!try_enable_x2apic_mode()) {
        if (directly_map_apic_page() != APIC_SUCCESS)  {
            pr_error("Setting up env for xapic operation failed");
            return APIC_ERROR;
        }
    }

    reset_lapic_state();
    set_task_priority_class(TP_CLASS_1);
    sw_enable_lapic();

    determine_apic_timer_frequency();

    return APIC_SUCCESS;
}

/* Expects the apic page to already be mapped in case we don't switch to x2apic mode */
int
setup_ap_apic(void)
{
    if (!is_lapic_present()) {
        return APIC_NOT_PRESENT;
    }

    try_enable_x2apic_mode();

    reset_lapic_state();
    set_task_priority_class(TP_CLASS_1);
    sw_enable_lapic();

    determine_apic_timer_frequency();

    return APIC_SUCCESS;
}

phys_addr_t
get_lapic_base(void)
{
    return APIC_MEM_BASE;
}

void
apic_signal_eoi(void)
{
    apic_write(APIC_EOI_REG, APIC_EOI_ACK);
}

