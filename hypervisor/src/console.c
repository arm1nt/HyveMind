#include "console.h"
#include "drivers/uart16550.h"

static int
init_serial_uart_dbg_console(dbg_console_t *console)
{
    if (uart16650_initialize() != 0) {
        return -1;
    }

    console->initialized = 1;
    console->ops.putc = &uart16550_putc;
    console->ops.puts = &uart16550_puts;

    return 0;
}

int
init_console(dbg_console_t *console)
{
    return init_serial_uart_dbg_console(console);
}

