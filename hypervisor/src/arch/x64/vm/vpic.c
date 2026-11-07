#include "fatal.h"
#include "halloc.h"
#include "string.h"
#include "types.h"
#include "vm.h"
#include "asm/apic.h"
#include "asm/irq_vectors.h"
#include "vm/vpic.h"

static void __print_vpic_dev_state(struct __vpic *vpic);

struct vpic *
allocate_vpic(void)
{
    struct vpic *vpic = (struct vpic *) hmalloc(sizeof(struct vpic));
    if (!vpic)
        return NULL;

    memset(vpic, 0, sizeof(struct vpic));
    spinlock_init(&vpic->lock);

    vpic->vpic1.state = VPIC_UNINITIALIZED;
    vpic->vpic1.lock = &vpic->lock;
    vpic->vpic2.state = VPIC_UNINITIALIZED;
    vpic->vpic2.lock = &vpic->lock;

    vpic->vpic1.sub_device_map = U8_LSHIFT(1, 2);
    vpic->vpic2.parent_vpic = &vpic->vpic1;

    return vpic;
}

void
destroy_vpic(struct vm *vm)
{
    struct vpic *vpic = vm_vpic(vm);
    if (!vpic)
        return;

    hfree(vpic);
    vm_vpic(vm) = NULL;
}

static inline bool
in_init_sequence(const struct __vpic *vpic)
{
    return vpic->state != VPIC_UNINITIALIZED && vpic->state != VPIC_INITIALIZED;
}

static inline enum vpic_state
get_next_init_state(const enum vpic_state current_state)
{
    switch (current_state) {
        case VPIC_ICW2:
        case VPIC_ICW3:
        case VPIC_ICW4:
            return VPIC_INITIALIZED;
        case VPIC_ICW2_ICW3:
            return VPIC_ICW3;
        case VPIC_ICW2_ICW4:
            return VPIC_ICW4;
        case VPIC_ICW2_ICW3_ICW4:
            return VPIC_ICW3_ICW4;
        case VPIC_ICW3_ICW4:
            return VPIC_ICW4;
        default:
            die_reason("Unreachable vPIC init state");
    }
}

static inline enum vpic_state
get_init_state_from_icw1(const uint8_t icw1)
{
    if (!ICW1_SETS(icw1, ICW1_IC4) && ICW1_SETS(icw1, ICW1_SNGL)) {
        return VPIC_ICW2;
    } else if (ICW1_SETS(icw1, ICW1_IC4) && !ICW1_SETS(icw1, ICW1_SNGL)) {
        return VPIC_ICW2_ICW3_ICW4;
    } else if (!ICW1_SETS(icw1, ICW1_IC4)) {
        return VPIC_ICW2_ICW3;
    } else {
        return VPIC_ICW2_ICW4;
    }
}

static inline void
start_init_sequence(struct __vpic *vpic, const uint8_t icw1)
{
    if (ICW1_UNSUPPORTED(icw1)) {
        die_reason("Unsupported vPIC feature requested via ICW1");
    }

    vpic->state = get_init_state_from_icw1(icw1);
    vpic->imr = 0;
    vpic->read_mode = VPIC_READ_IRR;

    if (vpic->parent_vpic) {
        vpic->sub_device_id = 7;
    } else {
        vpic->sub_device_map = U8_LSHIFT(1, 7);
    }
}

static void
handle_init_step(struct __vpic *vpic, const uint8_t val)
{
    switch (vpic->state) {
        case VPIC_ICW2:
        case VPIC_ICW2_ICW3:
        case VPIC_ICW2_ICW4:
        case VPIC_ICW2_ICW3_ICW4:
            vpic->offset = val & ~U8(7);
            break;
        case VPIC_ICW3:
        case VPIC_ICW3_ICW4:
            if (vpic->parent_vpic) {
                vpic->sub_device_id = val & U8(7);
            } else {
                vpic->sub_device_map = val;
            }
            break;
        case VPIC_ICW4:
            if (ICW4_UNSUPPORTED(val))
                die_reason("Unsupported vPIC feature requested via ICW4");
            break;
        default:
            die_reason("Unreachable");
    }

    vpic->state = get_next_init_state(vpic->state);
}

static inline void
handle_ocw2_write(struct __vpic *vpic, const uint8_t ocw2)
{
    if (OCW2_UNSUPPORTED(ocw2)) {
        pr_error("Requested unsupported OCW2: 0x%lx", ocw2);
        __print_vpic_dev_state(vpic);
        die_reason("Unsupported vPIC operation requested via OCW2");
    }

    const int cmd = ocw2 >> 5;
    switch (cmd) {
        case OCW2_NON_SPECIFIC_EOI_CMD:
            /* Reset highest priority bit set in ISR (0 is highest) */
            vpic->isr = (vpic->isr - 1) & vpic->isr;
            break;
        case OCW2_SPECIFIC_EOI_CMD:
            uint8_t mask = U8_LSHIFT(1, ocw2 & 0x3);
            vpic->isr &= (~mask);
            break;
        default:
            pr_error("Unsupported OCW2 command '%lu'", cmd);
            die();
    }
}

static inline void
handle_ocw3_write(struct __vpic *vpic, const uint8_t ocw3)
{
    if (OCW3_UNSUPPORTED(ocw3)) {
        pr_error("Requested unsupported OCW3: 0x%lx", ocw3);
        __print_vpic_dev_state(vpic);
        die_reason("Unsupported vPIC operation requested via OCW3");
    }

    if (!OCW3_SETS(ocw3, OCW3_RR)) {
        vpic->read_mode = VPIC_READ_NO_ACTION;
    } else {
        if (OCW3_SETS(ocw3, OCW3_RIS)) {
            vpic->read_mode = VPIC_READ_ISR;
        } else {
            vpic->read_mode = VPIC_READ_IRR;
        }
    }
}

void
vpic_write(struct __vpic *vpic, const uint16_t port, const uint8_t val)
{
    uint64_t flags;
    spinlock_acquire_irq_save(vpic->lock, flags);

    if (IS_ICW1(port, val)) {
        start_init_sequence(vpic, val);
        goto out;
    } else if (in_init_sequence(vpic)) {
        if (A0_CLEAR(port)) {
            pr_debug("Ignoring unexpected cmd received during the init sequence");
            goto out;
        }
        handle_init_step(vpic, val);
        goto out;
    }

    if (IS_OCW1(port, val)) {
        vpic->imr = val;
    } else if (IS_OCW2(port, val)) {
        handle_ocw2_write(vpic, val);
    } else if (IS_OCW3(port, val)) {
        handle_ocw3_write(vpic, val);
    } else {
        die_reason("Unreachable");
    }
out:
    spinlock_release_irq_restore(vpic->lock, flags);
}

void
vpic_read(struct __vpic *vpic, const uint16_t port, uint8_t *val)
{
    uint64_t flags;
    spinlock_acquire_irq_save(vpic->lock, flags);

    if (A0_CLEAR(port)) {
        if (vpic->read_mode == VPIC_READ_IRR) {
            *val = vpic->irr;
        } else if (vpic->read_mode == VPIC_READ_ISR) {
            *val = vpic->isr;
        }
    } else {
        *val = vpic->imr;
    }

    spinlock_release_irq_restore(vpic->lock, flags);
}

static inline struct vcpu *
get_irq_target_vcpu(const struct vm *vm)
{
    /**
     * For now, it is always true that vm->vcpus[0] is the target vCPU into
     * which to inject an irq
     */
    return vm->vcpus[0];
}

static bool
irq_target_vcpu_needs_kick(vcpu_t *vcpu)
{
    //NOT_YET_IMPLEMENTED - todo;
    return false;
}

static inline int
__vpic_raise_ext_irq(struct __vpic *vpic, const hwirq_t irq)
{
    if (vpic->state != VPIC_INITIALIZED)
        return -1;

    vpic->irr |= U8_LSHIFT(1, irq);
    return 0;
}

int
vpic_raise_external_irq(struct vm *vm, const guest_hwirq_t irq)
{
    int ret;
    struct vpic *vpic = vm_vpic(vm);

    if (irq >= VPIC_NUM_PINS)
        return -1;

    spinlock_acquire(&vpic->lock);

    if (irq >= 0x08)
        ret = __vpic_raise_ext_irq(&vm_vpic(vm)->vpic2, irq - 0x08);
    else
        ret = __vpic_raise_ext_irq(&vm_vpic(vm)->vpic1, irq);

    if (ret != 0)
        goto out;

    vcpu_t *target = get_irq_target_vcpu(vm);
    if (irq_target_vcpu_needs_kick(target)) {
        apic_send_targeted_ipi(vcpu_vmx(target).active_cpu, IRQ_CPU_KICK_VECTOR);
    }
out:
    spinlock_release(&vpic->lock);
    return ret;
}

static inline void
__vpic_ack(struct __vpic *vpic, const uint8_t irq)
{
    const uint8_t mask = U8_LSHIFT(1, irq);
    vpic->irr &= ~mask;
    vpic->isr |= mask;
}

static inline int
vpic_get_priority(const uint8_t val)
{
    if (val == 0)
        return 8;

    uint8_t prio = 0;
    while (!(val & U8_LSHIFT(1, prio)) && prio < 8)
        prio++;

    return (prio == 8) ? 8 : prio;
}

static inline int
__vpic_get_pending_irq(struct __vpic *vpic)
{
    const int pending_irq_prio = vpic_get_priority(vpic->irr & ~vpic->imr);
    if (pending_irq_prio == 8)
        return -1;

    const int curr_prio = vpic_get_priority(vpic->isr);
    if (pending_irq_prio < curr_prio) {
        return pending_irq_prio;
    } else {
        return -1;
    }
}

bool
vpic_has_pending_irq(struct vpic *vpic)
{
    spinlock_acquire(&vpic->lock);

    bool ret = false;
    const int irq1 = __vpic_get_pending_irq(&vpic->vpic1);
    const int irq2 = __vpic_get_pending_irq(&vpic->vpic2);
    const int vpic1_prio = vpic_get_priority(vpic->vpic1.isr);

    if (irq2 != -1) {
        if ((irq1 == -1 || irq1 > 2) && vpic1_prio > 2) {
            ret = true;
        }
    } else if (irq1 != -1) {
        ret = true;
    }

    spinlock_release(&vpic->lock);
    return ret;
}

/**
 * Returns the pending IRQ number (or -1) and acknowledges the interrupt
 */
int
vpic_get_pending_irq(struct vpic *vpic)
{
    spinlock_acquire(&vpic->lock);

    int intno = -1;
    const int irq1 = __vpic_get_pending_irq(&vpic->vpic1);
    const int irq2 = __vpic_get_pending_irq(&vpic->vpic2);
    const int vpic1_prio = vpic_get_priority(vpic->vpic1.isr);

    if (irq2 != -1) {
        if ((irq1 == -1 || irq1 > 2) && vpic1_prio > 2) {
            intno = irq2 + vpic->vpic2.offset;
            __vpic_ack(&vpic->vpic1, 2);
            __vpic_ack(&vpic->vpic2, irq2);
        }
    } else if (irq1 != -1) {
        intno = irq1 + vpic->vpic1.offset;
        __vpic_ack(&vpic->vpic1, irq1);
    }

    spinlock_release(&vpic->lock);
    return intno;
}

static inline char *
stringify_vpic_read_mode(const struct __vpic *vpic)
{
    switch (vpic->read_mode) {
        case VPIC_READ_IRR:
            return "read IRR";
        case VPIC_READ_ISR:
            return "read ISR";
        case VPIC_READ_NO_ACTION:
            return "no action (neither IRR nor ISR configured)";
    }
}

static inline char *
stringify_vpic_state(const struct __vpic *vpic)
{
    switch (vpic->state) {
        case VPIC_UNINITIALIZED:
            return "Uninitialized";
        case VPIC_INITIALIZED:
            return "Initialized";
        case VPIC_ICW2:
            return "In init sequence - expecting: ICW2";
        case VPIC_ICW3:
            return "In init sequence - expecting: ICW3";
        case VPIC_ICW4:
            return "In init sequence - expecting: ICW4";
        case VPIC_ICW2_ICW3:
            return "In init sequence - expecting: ICW2 -> ICW3";
        case VPIC_ICW2_ICW4:
            return "In init sequence - expecting: ICW2 -> ICW4";
        case VPIC_ICW2_ICW3_ICW4:
            return "In init sequence - expecting: ICW2 -> ICW3 -> ICW4";
        case VPIC_ICW3_ICW4:
            return "In init sequence - expecting: ICW3 -> ICW4";
    }
}

static void
__print_vpic_dev_state(struct __vpic *vpic)
{
    pr_info("State: %s", stringify_vpic_state(vpic));
    pr_info("IMR: 0x%lx", vpic->imr);
    pr_info("IRR: 0x%lx", vpic->irr);
    pr_info("ISR: 0x%lx", vpic->isr);
    pr_info("Offset: 0x%lx (=%lu)", vpic->offset, vpic->offset);
    pr_info("Read Mode: %s", stringify_vpic_read_mode(vpic));
    pr_info("Has parent vPIC: %s", ((vpic->parent_vpic == NULL) ? "no" : "yes"));
    if (vpic->parent_vpic) {
        pr_info("Parent vPIC address: 0x%lx", vpic->parent_vpic);
        pr_info("Sub-device ID: 0x%lx (=%lu)", vpic->sub_device_id, vpic->sub_device_id);
    } else {
        pr_info("Sub-device map: 0x%lx", vpic->sub_device_map);
    }
}

static inline void
print_vpic_dev_state(struct __vpic *vpic)
{
    spinlock_acquire(vpic->lock);
    __print_vpic_dev_state(vpic);
    spinlock_release(vpic->lock);
}

void
print_vpic_state(struct vpic *vpic)
{
    pr_warn("================== VPIC STATE START =====================");
    print_vpic_dev_state(&vpic->vpic1);
    print_vpic_dev_state(&vpic->vpic2);
    pr_warn("=================== VPIC STATE END ======================");
}

