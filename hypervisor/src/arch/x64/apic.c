#include "types.h"
#include "printf.h"
#include "asm/apic.h"
#include "asm/irq_vectors.h"
#include "asm/processor.h"
#include "asm/pgtables.h"

enum x2apic_register_msr {
    X2APIC_LAPIC_ID_REGISTER            = 0x802,
    X2APIC_TPR                          = 0x808,
    X2APIC_EOI_REGISTER                 = 0x80B,
    X2APIC_SVR                          = 0x80F,
    X2APIC_LVT_CMCI_REGISTER            = 0x82F,
    X2APIC_ICR                          = 0x830,
    X2APIC_LVT_TIMER_REGISTER           = 0x832,
    X2APIC_LVT_THERMAL_SENSOR_REGISTER  = 0x833,
    X2APIC_LVT_PERFMON_REGISTER         = 0x834,
    X2APIC_LVT_LINT0_REGISTER           = 0x835,
    X2APIC_LVT_LINT1_REGISTER           = 0x836,
    X2APIC_LVT_ERROR_REGISTER           = 0x837,
    X2APIC_INITIAL_COUNT_REGISTER       = 0x838,
    X2APIC_CURRENT_COUNT_REGISTER       = 0x839,
    X2APIC_DIVIDE_CONFIG_REGISTER       = 0x83E,
    X2APIC_SELF_IPI_REGISTER            = 0x83F,
};

#define LVT_ENTRY_MASK_BIT  16
#define LVT_ENTRY_MASK (U32_LSHIFT(1, LVT_ENTRY_MASK_BIT))

#define SVR_SOFTWARE_ENABLE_DISABLE_BIT 8
#define SVR_SOFTWARE_ENABLE_DISABLE (U32_LSHIFT(1, SVR_SOFTWARE_ENABLE_DISABLE_BIT))

/* Must be 0, any other value will cause a GP */
#define APIC_EOI_ACK 0x00

enum lapic_timer_type {
     TIMER_ONE_SHOT,
     TIMER_PERIODIC,
     TIMER_TSC_DEADLINE
};

enum icr_delivery_mode {
    DELIVERY_MODE_FIXED,
    DELIVERY_MODE_LOWEST_PRIO,
    DELIVERY_MODE_NMI = 0b100,
};

enum icr_destination_mode {
    DESTINATION_MODE_PHYSICAL,
    DESTINATION_MODE_LOGICAL,
};

static inline void
set_icr_vector(uint64_t *icr, const uint8_t vector)
{
    *icr = *icr | vector;
}

static inline void
set_icr_delivery_mode(uint64_t *icr, const enum icr_delivery_mode mode)
{
    *icr = *icr | (U64(mode) << 8);
}

static inline void
set_icr_destination_mode(uint64_t *icr, const enum icr_destination_mode mode)
{
    *icr = *icr | (U64(mode) << 11);
}

static inline void
set_icr_level(uint64_t *icr)
{
    *icr = *icr | (U64(1) << 14);
}

static inline void
set_icr_destination(uint64_t *icr, const lapicid_t target)
{
    *icr = *icr | (U64(target) << 32);
}

void
apic_send_self_ipi(const uint8_t vector)
{
    if (in_x2apic_mode()) {
        write_msr(X2APIC_SELF_IPI_REGISTER, vector);
    }
}

void
apic_send_targeted_sched_ipi(const lapicid_t target_id)
{
    apic_send_targeted_ipi_fixed(target_id, IRQ_SCHEDULE_VECTOR);
}

inline void
apic_send_targeted_ipi_fixed(const lapicid_t target_id, const uint8_t vector)
{
    uint64_t icr = 0;

    set_icr_vector(&icr, vector);
    set_icr_delivery_mode(&icr, DELIVERY_MODE_FIXED);
    set_icr_destination_mode(&icr, DESTINATION_MODE_PHYSICAL);
    set_icr_level(&icr);

    if (in_x2apic_mode()) {
        set_icr_destination(&icr, target_id);
        write_msr(X2APIC_ICR, icr);
    }
}

void
apic_signal_eoi(void)
{
    if (in_x2apic_mode()) {
        write_msr(X2APIC_EOI_REGISTER, APIC_EOI_ACK);
    }
}

inline void
apic_set_task_priority(const enum tpr_priority priority)
{
    write_cr8(priority);
}

static inline void
x2apic_unmask_lvt_entry(const enum x2apic_register_msr lvt_register_addr)
{
    const uint32_t lvt_entry = read_msr(lvt_register_addr);
    write_msr(lvt_register_addr, lvt_entry & (~LVT_ENTRY_MASK));
}

static inline void
x2apic_mask_lvt_entry(const enum x2apic_register_msr lvt_register_addr)
{
    const uint32_t lvt_entry = read_msr(lvt_register_addr);
    write_msr(lvt_register_addr, lvt_entry | LVT_ENTRY_MASK);
}

static inline void
x2apic_set_lvt_entry_vector(const enum x2apic_register_msr reg, const uint8_t vector)
{
    uint32_t lvt_entry = read_msr(reg);
    lvt_entry = ((lvt_entry >> 12) << 12) | vector;
    write_msr(reg, lvt_entry);
}

static inline void
x2apic_lvt_timer_set_type(const enum lapic_timer_type timer_type)
{
    const uint32_t clear_mask = ~(U32(3) << 17);
    uint32_t lvt_timer = read_msr(X2APIC_LVT_TIMER_REGISTER);

    lvt_timer &= clear_mask;
    lvt_timer |= (U32(timer_type) << 17);
    write_msr(X2APIC_LVT_TIMER_REGISTER, lvt_timer);
}

static inline void
x2apic_set_div_config_value(const enum dcr_value div_value)
{
    const uint8_t leading_bit = div_value >> 2;
    const uint8_t trailing_bits = div_value & 3;

    write_msr(X2APIC_DIVIDE_CONFIG_REGISTER, (leading_bit << 3) | trailing_bits);
}

void
apic_start_one_shot_timer(const uint32_t initial_count, const enum dcr_value div)
{
    if (in_x2apic_mode()) {
        x2apic_set_lvt_entry_vector(X2APIC_LVT_TIMER_REGISTER, APIC_ONESHOT_TIMER_VECTOR);
        x2apic_lvt_timer_set_type(TIMER_ONE_SHOT);

        write_msr(X2APIC_INITIAL_COUNT_REGISTER, 0);
        x2apic_set_div_config_value(div);

        x2apic_unmask_lvt_entry(X2APIC_LVT_TIMER_REGISTER);

        /* arm the timer */
        write_msr(X2APIC_INITIAL_COUNT_REGISTER, initial_count);
    }
}

void
apic_start_periodic_timer(const uint64_t start_count, const enum dcr_value div)
{
    if (in_x2apic_mode()) {
        x2apic_set_lvt_entry_vector(X2APIC_LVT_TIMER_REGISTER, APIC_PERIODIC_TIMER_VECTOR);
        x2apic_lvt_timer_set_type(TIMER_PERIODIC);

        write_msr(X2APIC_INITIAL_COUNT_REGISTER, 0);
        x2apic_set_div_config_value(div);

        x2apic_unmask_lvt_entry(X2APIC_LVT_TIMER_REGISTER);

        /* arm the timer */
        write_msr(X2APIC_INITIAL_COUNT_REGISTER, start_count);
    }
}

inline void
apic_mask_all_local_interrupts(void)
{
    if (in_x2apic_mode()) {
        /* TODO: Check whether CMCI LVT entry exists */
        x2apic_mask_lvt_entry(X2APIC_LVT_TIMER_REGISTER);
        x2apic_mask_lvt_entry(X2APIC_LVT_THERMAL_SENSOR_REGISTER);
        x2apic_mask_lvt_entry(X2APIC_LVT_PERFMON_REGISTER);
        x2apic_mask_lvt_entry(X2APIC_LVT_LINT0_REGISTER);
        x2apic_mask_lvt_entry(X2APIC_LVT_LINT1_REGISTER);
        x2apic_mask_lvt_entry(X2APIC_LVT_ERROR_REGISTER);
    }
}

static inline void
set_spurious_interrupt_vector(void)
{
    if (in_x2apic_mode()) {
        uint32_t svr = read_msr(X2APIC_SVR);
        svr = (svr >> 8) << 8;
        svr |= U8(APIC_SPURIOUS_VECTOR);
        write_msr(X2APIC_SVR, svr);
    }
}

static inline void
x2apic_software_enable(void)
{
    const uint32_t svr = read_msr(X2APIC_SVR);
    write_msr(X2APIC_SVR, svr | SVR_SOFTWARE_ENABLE_DISABLE);
}

static inline void
x2apic_software_disable(void)
{
    const uint32_t svr = read_msr(X2APIC_SVR);
    write_msr(X2APIC_SVR, svr & (~SVR_SOFTWARE_ENABLE_DISABLE));
}

inline void
apic_software_enable(void)
{
    if (in_x2apic_mode()) {
        x2apic_software_enable();
    }
}

inline void
apic_software_disable(void)
{
    if (in_x2apic_mode()) {
        x2apic_software_disable();
    }
}

static inline bool
x2apic_mode_supported(void)
{
    if (!cpuid_leaf_in_range(CPUID_CPU_FEATURES_LEAF)) {
        goto out_not_supported;
    }

    const cpuid_result_t result = cpuid_raw(CPUID_CPU_FEATURES_LEAF, NO_SUBLEAF_INDEX);
    if (IS_SET(result.ecx, CPUID_X2APIC_MODE)) {
        pr_debug("x2apic_mode_supported");
        return true;
    }

out_not_supported:
    pr_debug("x2apic mode not supported");
    return false;
}

static inline bool
enable_x2apic_mode(void)
{
    const uint64_t apic_base = read_msr(MSRX64_IA32_APIC_BASE);

    if (IS_CLEAR(apic_base, MSR_APIC_BASE_EN)) {
        /* en = 0, extd = 1 is invalid */
        pr_warn("Cannot enable x2apic mode as the local apic is disabled");
        return false;
    }

    write_msr(MSRX64_IA32_APIC_BASE, apic_base | MSR_APIC_BASE_EXTD);
    return true;
}

static bool
x2apic_setup_local_apic(void)
{
    bool ret;

    ret = enable_x2apic_mode();
    if (!ret) {
        return ret;
    }

    apic_mask_all_local_interrupts();
    set_spurious_interrupt_vector();
    /* Disallow the first two irq priority classes (vectors 0-31) from being delivered */
    apic_set_task_priority(TPR_PRIO1);
    /* Ensure we are enabled */
    apic_software_enable();

    return true;
}

static inline bool
xapic_setup_local_apic(void)
{
    /**
     * We mainly focus on x2apic mode, especially since xapic mode is being
     * deprecated.
     */
    const phys_addr_t lapic_base = get_lapic_base();
    if (identity_map_mmio_page(lapic_base) != 0) {
        pr_error("Unable to map lapic page");
        return false;
    }

    return false;
}

bool
setup_local_apic(void)
{
    if (x2apic_mode_supported()) {
        return x2apic_setup_local_apic();
    } else {
        return xapic_setup_local_apic();
    }
}

