#include "printf.h"
#include "console.h"

static dbg_console_t console = { .initialized = 0 };

int
__printf(const char *fmt, ...)
{
    if (!console.initialized) {
        return -1;
    }

    console.ops.puts((char *)fmt);

    return 0;
}

int
init_printf(void)
{
    if (init_console(&console) != 0) {
        return -1;
    }

    return 0;
}

