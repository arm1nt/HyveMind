#include "logger.h"
#include "drivers/uart16550.h"

struct debug_console_ops {
    void (*putc)(char);
    void (*puts)(char *);
};

struct debug_print_console {
    int initialized;
    struct debug_console_ops ops;
};
typedef struct debug_print_console debug_print_console_t;

static debug_print_console_t console = { .initialized = 0 };

static int
init_serial_uart_debug_console(void)
{
    if (uart16650_initialize()) {
        return -1;
    }
    console.initialized = 1;

    console.ops.putc = &uart16550_putc;
    console.ops.puts = &uart16550_puts;

    return 0;
}

void
debug_printf(const char *fmt, ...)
{
    if (!console.initialized) {
        return;
    }

    // TODO: do properly, current impl is just for testing
    console.ops.puts((char *)fmt);
}

int
init_debug_logging(void)
{
    return init_serial_uart_debug_console();
}

