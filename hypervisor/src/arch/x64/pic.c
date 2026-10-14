#include "hyvstdlib.h"
#include "spinlock.h"
#include "types.h"
#include "io.h"
#include "asm/irq.h"
#include "asm/irq_vectors.h"

#define PIC1        0x0020
#define PIC2        0x00A0
#define PIC1_CMD    PIC1
#define PIC1_DATA   (PIC1 + 1)
#define PIC2_CMD    PIC2
#define PIC2_DATA   (PIC2 + 1)

#define PIC1_CASCADE_IR 0x02
#define PIC2_CASCADE_ID PIC1_CASCADE_IR

#define PIC_EOI_CMD 0x20

// D4=1 identifies cmd as ICW1 and starts init sequence
#define ICW1_BASE               0x0010
#define ICW1_ICW4_NEEDED        0x0001
#define ICW1_SINGLE             0x0002
#define ICW1_LEVEL_TRIGGERED    0x0008

#define ICW4_8086_MODE          0x0001

#define OCW3_BASE               0x0008
#define OCW3_READ_IRR           0x0002
#define OCW3_READ_ISR           0x0003

DEFINE_SPINLOCK(pic_lock);

static inline uint8_t
read_isr(const uint16_t target_port)
{
    io_outb(target_port, OCW3_BASE | OCW3_READ_ISR);
    return io_inb(target_port);
}

static void
mask_irq(const hwirq_t device_irq)
{
    uint64_t flags;
    spinlock_acquire_irq_save(&pic_lock, flags);

    const uint16_t target_port = (device_irq < 8) ? PIC1_DATA : PIC2_DATA;
    const uint8_t val = (device_irq < 8) ? device_irq : device_irq - 8;

    const uint8_t masked_imr = io_inb(target_port) | U8_LSHIFT(1, val);
    io_outb(target_port, masked_imr);
    spinlock_release_irq_restore(&pic_lock, flags);
}

static void
unmask_irq(const hwirq_t device_irq)
{
    uint64_t flags;
    spinlock_acquire_irq_save(&pic_lock, flags);

    const uint16_t target_port = (device_irq < 8) ? PIC1_DATA : PIC2_DATA;
    const uint8_t val = (device_irq < 8) ? device_irq : device_irq - 8;

    const uint8_t unmasked_imr = io_inb(target_port) & ~U8_LSHIFT(1, val);
    io_outb(target_port, unmasked_imr);
    spinlock_release_irq_restore(&pic_lock, flags);
}

static bool
is_spurious(const irq_t irq_vec)
{
    spinlock_acquire(&pic_lock);
    const hwirq_t device_irq = irq_vec - IRQ_LEGACY_BASE_VECTOR;
    const uint16_t target_port = (device_irq < 8) ? PIC1_CMD : PIC2_CMD;
    const uint8_t val = (device_irq < 8) ? device_irq : device_irq - 8;
    const uint8_t isr = read_isr(target_port);
    spinlock_release(&pic_lock);

    return IS_CLEAR(isr, U8_LSHIFT(1, val));
}

static void
spurious_eoi(const irq_t irq_vec)
{
    spinlock_acquire(&pic_lock);
    if (irq_vec >= IRQ_LEGACY_VECTOR(8)) {
        io_outb(PIC1_CMD, PIC_EOI_CMD);
    }
    spinlock_release(&pic_lock);
}

static void
signal_eoi(const uint8_t hw_irq)
{
    spinlock_acquire(&pic_lock);
    if (hw_irq >= 8) {
        io_outb(PIC2_CMD, PIC_EOI_CMD);
    }

    io_outb(PIC1_CMD, PIC_EOI_CMD);
    spinlock_release(&pic_lock);
}

static void
init_pic(void)
{
    io_outb(PIC1_CMD, ICW1_BASE | ICW1_ICW4_NEEDED);
    io_outb(PIC1_DATA, IRQ_PIC1_BASE_VECTOR);
    io_outb(PIC1_DATA, 1 << PIC1_CASCADE_IR);
    io_outb(PIC1_DATA, ICW4_8086_MODE);

    io_outb(PIC2_CMD, ICW1_BASE | ICW1_ICW4_NEEDED);
    io_outb(PIC2_DATA, IRQ_PIC2_BASE_VECTOR);
    io_outb(PIC2_DATA, PIC2_CASCADE_ID);
    io_outb(PIC2_DATA, ICW4_8086_MODE);

    // Mask every IRQ until we explicitly want to use them
    io_outb(PIC1_DATA, 0xFF);
    io_outb(PIC2_DATA, 0xFF);
}

struct irq_domain legacy_pic_domain = {
    .init = init_pic,
    .eoi = signal_eoi,
    .is_spurious = is_spurious,
    .spurious_eoi = spurious_eoi,
    .mask_irq_line = mask_irq,
    .unmask_irq_line = unmask_irq,
};

