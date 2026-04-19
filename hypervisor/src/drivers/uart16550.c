#include <stdint.h>

#include "drivers/uart16550.h"
#include "io.h"
#include "types.h"

#define DEFAULT_SERIAL_PORT_BASE 0x3F8

#define SP_RCV_BUFFER_OFFSET            (0)
#define SP_TX_HOLDING_OFFSET            (0)
#define SP_INTERRUPT_ENABLE_OFFSET      (1)
#define SP_FIFO_CONTROL_OFFSET          (2)
#define SP_LINE_CONTROL_OFFSET          (3)
#define SP_MODEM_CONTROL_OFFSET         (4)
#define SP_LINE_STATUS_OFFSET           (5)
#define SP_MODEM_STATUS_OFFSET          (6)
#define SP_SCRATCH_OFFSET               (7)

/* They share I/O addresses, access is controlled via the DLAB bit */
#define SP_DIVISOR_MSB_OFFSET           SP_INTERRUPT_ENABLE_OFFSET
#define SP_DIVISOR_LSB_OFFSET           SP_RCV_BUFFER_OFFSET

#define DLAB_MASK       U8_LSHIFT(0x01, 7)
#define LOOPBACK_MASK   U8_LSHIFT(0x01, 4)

#define UART_NO_INTERRUPTS  (0x00)
#define UART_FIFO_ENABLE            U8_LSHIFT(0x01, 0)
#define UART_FIFO_CLEAR_RCV_BUFFER  U8_LSHIFT(0x01, 1)
#define UART_FIFO_CLEAR_TX_BUFFER   U8_LSHIFT(0x01, 2)

#define UART_TRANSMISSION_BUFFER_EMPTY(line_status_reg) \
    (line_status_reg & U8_LSHIFT(0x01, 5))

static int serial_uart_base_port = DEFAULT_SERIAL_PORT_BASE;

static inline int
loopback_test(void)
{
    uint8_t mcr, read;

    mcr = io_inb(serial_uart_base_port + SP_MODEM_CONTROL_OFFSET);
    io_outb(serial_uart_base_port + SP_MODEM_CONTROL_OFFSET, mcr | LOOPBACK_MASK);

    /* Ensure we receive the value we transmit */
    io_outb(serial_uart_base_port + SP_TX_HOLDING_OFFSET, 0x03);
    read = io_inb(serial_uart_base_port + SP_RCV_BUFFER_OFFSET);

    io_outb(serial_uart_base_port + SP_MODEM_CONTROL_OFFSET, mcr);

    return  (read == 0x03) ? 0 : -1;
}

static inline void
set_protocol_parameters(void)
{
    // 8n1 config
    uint8_t base_params = 0x00;

    base_params |= 0x03; /* 8 data bits per frame */
    base_params |= U8_LSHIFT(0x00, 2);
    base_params |= U8_LSHIFT(0x00, 3);

    io_outb(serial_uart_base_port + SP_LINE_CONTROL_OFFSET, base_params);
}

static inline void
set_baud_rate(void)
{
    uint8_t lcr;

    lcr = io_inb(serial_uart_base_port + SP_LINE_CONTROL_OFFSET);
    io_outb(serial_uart_base_port + SP_LINE_CONTROL_OFFSET, lcr | DLAB_MASK);

    /* Set divisor to 1 per default */
    io_outb(serial_uart_base_port + SP_DIVISOR_LSB_OFFSET, 0x01);
    io_outb(serial_uart_base_port + SP_DIVISOR_MSB_OFFSET, 0x00);

    io_outb(serial_uart_base_port + SP_LINE_CONTROL_OFFSET, lcr);
}

int
uart16650_initialize(void)
{
    set_baud_rate();
    set_protocol_parameters();

    io_outb(serial_uart_base_port + SP_INTERRUPT_ENABLE_OFFSET, UART_NO_INTERRUPTS);

    io_outb(
            serial_uart_base_port + SP_FIFO_CONTROL_OFFSET,
            UART_FIFO_ENABLE | UART_FIFO_CLEAR_RCV_BUFFER | UART_FIFO_CLEAR_TX_BUFFER
    );

    /* Enable DTR and RTS */
    io_outb(serial_uart_base_port + SP_MODEM_CONTROL_OFFSET, 0x03);

    return loopback_test();
}

inline void
uart16550_putc(char c)
{
    uint8_t line_status;

    do {
        line_status = io_inb(serial_uart_base_port + SP_LINE_STATUS_OFFSET);
        asm volatile ("pause");
    } while (!UART_TRANSMISSION_BUFFER_EMPTY(line_status));

    io_outb(serial_uart_base_port + SP_TX_HOLDING_OFFSET, c);
}

inline void
uart16550_puts(char *s)
{
    if (s == (char *)0) {
        return;
    }

    int index = 0;

    while(s[index] != '\0') {
        uart16550_putc(s[index]);
        index++;
    }
}

