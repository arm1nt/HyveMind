#include "io.h"
#include "vm.h"
#include "vm/io.h"

/**
 * We provide the same set of devices behind the same ports for each VM
 */
#define IO_UART_START          0x3F8
#define IO_UART_END            0x3FF
#define IO_PIT_START           0x040
#define IO_PIT_END             0x043
#define IO_PIC_PRIMARY_START   0x020
#define IO_PIC_PRIMARY_END     0x021
#define IO_PIC_SECONDARY_START 0x0A0
#define IO_PIC_SECONDARY_END   0x0A1

#define passthru_serial_port(vcpu, port) \
    (vm_devices((vcpu)->vm).host_serial_pio_base + ((port) - IO_UART_START))

static inline void
io_passthru_read(const struct pio_operation *pio, struct vcpu_user_regs *regs)
{
    switch (pio->size) {
        case PIO_OP_SIZE_BYTE:
            regs->rax = U8(io_inb(pio->port));
            break;
        case PIO_OP_SIZE_WORD:
            regs->rax = U16(io_inw(pio->port));
            break;
        case PIO_OP_SIZE_DWORD:
            regs->rax = U32(io_inl(pio->port));
            break;
    }
}

static inline void
io_passthru_write(const struct pio_operation *pio, const struct vcpu_user_regs *regs)
{
    switch (pio->size) {
        case PIO_OP_SIZE_BYTE:
            io_outb(pio->port, U8(regs->rax));
            break;
        case PIO_OP_SIZE_WORD:
            io_outw(pio->port, U16(regs->rax));
            break;
        case PIO_OP_SIZE_DWORD:
            io_outl(pio->port, U32(regs->rax));
            break;
    }
}

static void
handle_guest_io_read(struct pio_operation *pio, vcpu_t *vcpu)
{
    struct vcpu_user_regs *regs = &vcpu_state(vcpu).user_regs;

    switch (pio->port) {
        case IO_UART_START ... IO_UART_END:
            pio->port = passthru_serial_port(vcpu, pio->port);
            io_passthru_read(pio, regs);
            break;
        case IO_PIT_START ... IO_PIT_END:
            pr_warn("Intercepted PIT read");
            vpit_read(vm_vpit(vcpu->vm), pio->port, (uint8_t *) &regs->rax);
            break;
        case IO_PIC_PRIMARY_START ... IO_PIC_PRIMARY_END:
            vpic_read(&vm_vpic(vcpu->vm)->vpic1, pio->port, (uint8_t *) &regs->rax);
            break;
        case IO_PIC_SECONDARY_START ... IO_PIC_SECONDARY_END:
            vpic_read(&vm_vpic(vcpu->vm)->vpic2, pio->port, (uint8_t *) &regs->rax);
            break;
    }
}

static void
handle_guest_io_write(struct pio_operation *pio, vcpu_t *vcpu)
{
    struct vcpu_user_regs *regs = &vcpu_state(vcpu).user_regs;

    switch (pio->port) {
        case IO_UART_START ... IO_UART_END:
            pio->port = passthru_serial_port(vcpu, pio->port);
            io_passthru_write(pio, regs);
            break;
        case IO_PIT_START ... IO_PIT_END:
            pr_warn("Intercepted PIT write");
            vpit_write(vm_vpit(vcpu->vm), pio->port, regs->rax);
            break;
        case IO_PIC_PRIMARY_START ... IO_PIC_PRIMARY_END:
            vpic_write(&vm_vpic(vcpu->vm)->vpic1, pio->port, regs->rax);
            break;
        case IO_PIC_SECONDARY_START ... IO_PIC_SECONDARY_END:
            vpic_write(&vm_vpic(vcpu->vm)->vpic2, pio->port, regs->rax);
            break;
    }
}

void
handle_guest_io_op(struct pio_operation *pio, struct vcpu *vcpu)
{
    switch (pio->direction) {
        case PIO_OP_READ:
            handle_guest_io_read(pio, vcpu);
            break;
        case PIO_OP_WRITE:
            handle_guest_io_write(pio, vcpu);
            break;
    }
}

