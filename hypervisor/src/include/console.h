#ifndef _HYVEMIND_CONSOLE_H
#define _HYVEMIND_CONSOLE_H

#include <stdint.h>

enum console_type {
    SERIAL_UART,
};
typedef enum console_type console_type_t;

struct console_ops {
    void (*putc)(char);
    void (*puts)(char *restrict);
};
typedef struct console_ops console_ops_t;

struct dbg_console {
    console_type_t type;
    uint8_t initialized;
    console_ops_t ops;
};
typedef struct dbg_console dbg_console_t;

int init_console(dbg_console_t *console);

#endif /* _HYVEMIND_CONSOLE_H */

