#include "printf.h"
#include "asm/cpu_family.h"
#include "asm/cpufeatures.h"
#include "asm/processor.h"
#include "asm/timer.h"
#include "drivers/hpet.h"

static uint64_t tsc_hz;

static uint64_t
get_scalable_bus_frequency(void)
{
    const cpuid_result_t res = cpuid_raw(CPUID_CPU_VERSION_LEAF, NO_SUBLEAF_INDEX);
    const int model_id = (res.eax >> CPUID_MODEL_ID_SHIFT) & CPUID_MODEL_ID_MASK;
    const int family_id = (res.eax >> CPUID_FAMILY_ID_SHIFT) & CPUID_FAMILY_ID_MASK;

    if (family_id == INTEL_FAMILY_6) {
        switch (model_id) {
            case INTEL_SANDYBRIDGE_MODEL_ID:
            case INTEL_SANDYBRIDGE_X_MODEL_ID:
            case INTEL_IVYBRIDGE_MODEL_ID:
            case INTEL_IVYBRIDGE_X_MODEL_ID:
            case INTEL_HASWELL_MODEL_ID:
            case INTEL_HASWELL_X_MODEL_ID:
            case INTEL_HASWELL_L_MODEL_ID:
            case INTEL_HASWELL_G_MODEL_ID:
            case INTEL_BROADWELL_MODEL_ID:
            case INTEL_BROADWELL_X_MODEL_ID:
            case INTEL_BROADWELL_L_MODEL_ID:
            case INTEL_BROADWELL_G_MODEL_ID:
                return 100;
            case INTEL_NEHALEM_MODEL_ID:
            case INTEL_NEHALEM_G_MODEL_ID:
            case INTEL_NEHALEM_EP_MODEL_ID:
            case INTEL_NEHALEM_EX_MODEL_ID:
                return 133;
            default:
                goto error_out;
        }
    }

error_out:
    pr_debug("No default bus frequency known for processor with family id "
             "'%lu' and model id '%lu'.",
             family_id,
             model_id
    );

    return 0;
}

/**
 * How we have to compute the TSC frequency heavily depends on the system.
 *
 * - Invariant TSC with ART:
 *      Here the TSC frequency is based on the ART hardware which runs at core
 *      crystal clock frequency. Namely:
 *          TSC_HZ = (tsc/core_crystal_clock)_ratio * core_crystal_clock_frequency
 *
 * - Invariant TSC without ART:
 *      According to the SDM, it always holds true that the TSC operates close
 *      to the maximum non-turbo frequency, which is equal to the product of
 *      the scalable_bus_frequency and the maximum non-turbo ratio, i.e.:
 *          TSC_HZ ~= bus_frequency * max_non_turbo_ratio
 */
static int
compute_tsc_frequency(void)
{
    if (cpuid_leaf_in_range(CPUID_TSC_CRYSTAL_CLOCK_LEAF)) {
        const cpuid_result_t cpuid_res =
            cpuid_raw(CPUID_TSC_CRYSTAL_CLOCK_LEAF, NO_SUBLEAF_INDEX);

        if (cpuid_res.ebx == 0 || cpuid_res.ecx == 0) {
            pr_debug("ART hardware values not enumerated");
            goto no_art;
        }

        pr_debug("Computing TSC frequency based on ART info in CPUID.0x15");

        tsc_hz = (cpuid_res.ecx * cpuid_res.ebx) / cpuid_res.eax;
        goto out_success;
    }

no_art:
    pr_debug("No ART hardware present. Trying to compute the TSC frequency based "
            "on bus frequency and max non-turbo ratio info from MSRs"
    );

    const uint64_t platform_info = read_msr(MSR_PLATFORM_INFO);
    const uint64_t max_non_turbo_ratio = (platform_info >> 8) & 0xFF;

    if (max_non_turbo_ratio == 0) {
        /**
         * KVM does not pass through the actual MSR_PLATFORM_INFO value, but
         * instead it injects a dummy value that provides no info
         */
        pr_debug("The platform info MSR does not provide useful information");
        goto no_msr_info;
    }

    const uint64_t scalable_bus_frequency = get_scalable_bus_frequency();
    if (scalable_bus_frequency == 0) {
        pr_debug("No default bus frequency value known for the processor");
        goto no_msr_info;
    }

    pr_debug("Computing TSC frequency based on MSR_PLATFORM_INFO information");
    tsc_hz = max_non_turbo_ratio * scalable_bus_frequency;
    goto out_success;

no_msr_info:
    pr_debug("Falling back to using the HPET timer to compute the TSC frequency");

    if (init_hpet() != HPET_SUCCESS) {
        pr_error("Unable to compute the TSC frequency using HPET");
        goto out_error;
    }

    const uint64_t tsc_start = read_tsc();
    hpet_do_busy_sleep(100000000); /* sleep for 100ms */
    const uint64_t tsc_end = read_tsc();

    tsc_hz = (tsc_end - tsc_start) * 10;

out_success:
    pr_info("TSC is running at %lu HZ", tsc_hz);
    return TIMER_SUCCESS;

out_error:
    pr_error("Unable to determine the TSC frequency");
    return TIMER_ERROR;
}

int
init_timer(void)
{
    if (compute_tsc_frequency() != TIMER_SUCCESS) {
        goto out_error;
    }

    return TIMER_SUCCESS;

out_error:
    pr_error("Failed to initialize system timer");
    return TIMER_INIT_FAILED;
}

