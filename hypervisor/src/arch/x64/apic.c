#include "types.h"
#include "printf.h"
#include "asm/apic.h"
#include "asm/irq_vectors.h"
#include "asm/processor.h"

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
    X2APIC_SELF_IPI_REGISTER            = 0x83F,
};

#define LVT_ENTRY_MASK_BIT  16
#define LVT_ENTRY_MASK (U32_LSHIFT(1, LVT_ENTRY_MASK_BIT))

#define SVR_SOFTWARE_ENABLE_DISABLE_BIT 8
#define SVR_SOFTWARE_ENABLE_DISABLE (U32_LSHIFT(1, SVR_SOFTWARE_ENABLE_DISABLE_BIT))

#define APIC_EOI_ACK 0x01

enum lapic_timer_type {
     TIMER_ONE_SHOT,
     TIMER_PERIODIC,
     TIMER_TSC_DEADLINE
};

void
apic_signal_eoi(void)
{
    if (in_x2apic_mode()) {
        write_msr(X2APIC_EOI_REGISTER, APIC_EOI_ACK);
    }
}

void
apic_send_targeted_ipi_fixed(const lapicid_t target_id, const uint8_t vector)
{
    return;
}

inline void
apic_set_task_priority(const int priority_class)
{
    write_cr8(priority_class);
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
    return;
}

void
start_one_shot_apic_timer(const uint64_t initial_count, const enum dcr_value div)
{
    x2apic_set_lvt_entry_vector(X2APIC_LVT_TIMER_REGISTER, APIC_TIMER_TEST_VECTOR);
    x2apic_lvt_timer_set_type(TIMER_ONE_SHOT);

    return;
}

void
start_periodic_apic_timer(const uint64_t start_count, const enum dcr_value div)
{
    x2apic_set_lvt_entry_vector(X2APIC_LVT_TIMER_REGISTER, APIC_TIMER_TEST_VECTOR);
    x2apic_lvt_timer_set_type(TIMER_PERIODIC);

    return;
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
        svr = svr & U8(0);
        svr |= APIC_SPURIOUS_VECTOR;
        write_msr(X2APIC_SVR, svr);
    }
}

static inline void
x2apic_software_enable(void)
{
    const uint32_t svr = read_msr(X2APIC_SVR);
    write_msr(X2APIC_SVR, svr | SVR_SOFTWARE_ENABLE_DISABLE);
}

inline void
apic_software_enable(void)
{
    if (in_x2apic_mode()) {
        x2apic_software_enable();
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
    apic_set_task_priority(0x01);

    /* Ensure we are enabled */
    apic_software_enable();

    return true;
}

static inline bool
xapic_setup_local_apic(void)
{
    /**
     * We'll focus on x2apic mode support, especially since xapic mode
     * is being deprecated.
     */
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

