#include "printf.h"
#include "console.h"

#include <stdarg.h>

static dbg_console_t console = { .initialized = 0 };

static void
u64_to_dec_string(uint64_t num, char *buf)
{
    int index = 0;
    while (num > 0) {
        buf[index++] = num % 10 + 48;
        num /= 10;
    }

    for (int i = 0; i < index/2; i++) {
        int temp = buf[index-i-1];
        buf[index-i-1] = buf[i];
        buf[i] = temp;
    }

    /* Ensure buf is null terminated */
    buf[index] = '\0';
}

static void
u64_to_hex_string(uint64_t num, char *buf)
{
    uint64_t check_zero_mask = ~((uint64_t) 0);
    uint64_t lsbs_mask = (uint64_t) 15; /* ...0001111 */
    int counter = 0;

    while (check_zero_mask & num) {
        char val = num & lsbs_mask;

        if (val >= 0 && val <= 9) {
            buf[counter] = val + 48;
        } else if (val >= 10 && val <= 15) {
            buf[counter] = (val - 10) + 65;
        }

        counter++;
        num >>= 4;
    }

    for(int i = 0; i < counter/2; i++) {
        char temp = buf[counter-i-1];
        buf[counter-i-1] = buf[i];
        buf[i] = temp;
    }

    /* Ensure buf is null terminated */
    buf[counter] = '\0';
}

int
__printf(const char *fmt, ...)
{
    if (!console.initialized) {
        return -1;
    }

    char c;
    uint64_t index = 0;
    va_list args;
    va_start(args, fmt);

    while (fmt[index] != '\0') {
        c = fmt[index];

        if (c != '%') {
            /* TODO: buffer writes instead of writing character by character */
            console.ops.putc(c);
            index++;
            continue;
        }

        char buf[512];
        switch (fmt[index + 1]) {
            case '%':
                /* Case makes the escaping explicit, the case could be omitted */
                index++;
                continue;
            case 's':
                console.ops.puts(va_arg(args, char*));
                index +=2;
                continue;
            case 'l':
                switch (fmt[index+2]) {
                    case 'u':
                        /* unsigned long in decimal form */
                        u64_to_dec_string(va_arg(args, uint64_t), buf);
                        console.ops.puts(buf);
                        index+=3;
                        continue;
                    case 'x':
                        /* unsigned long in hex form */
                        u64_to_hex_string(va_arg(args, uint64_t), buf);
                        console.ops.puts(buf);
                        index +=3;
                        continue;
                    case 'b':
                        /* unsigned long in binary form */
                        /* TODO: */
                        index +=3;
                        continue;
                }

            default:
                console.ops.putc(c);
                index++;
        }

    }

    /* TODO: return the number of characters written */
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

