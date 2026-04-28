#ifndef _HYVEMIND_PRINTF_H
#define _HYVEMIND_PRINTF_H

int init_printf(void);

#define PRINT_PREFIX_NAME "hyvemind"
#define PRINT_PREFIX "[" PRINT_PREFIX_NAME "]: "

/**
 * See docs/printf for a list & description of accepted format specifiers.
 */
#define printf(fmt, ...) __printf(PRINT_PREFIX fmt "\n" __VA_OPT__(,) __VA_ARGS__)
int __printf(const char *fmt, ...);

#endif /* _HYVEMIND_PRINTF_H */

