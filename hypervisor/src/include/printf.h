#ifndef _HYVEMIND_PRINTF_H
#define _HYVEMIND_PRINTF_H

#define PRINT_PREFIX_NAME "hyvemind"
#define PRINT_PREFIX "[" PRINT_PREFIX_NAME "]: "

int init_printf(void);

#ifdef HYVEMIND_USE_ANSI_COLORING

#define COLOR_START_GREEN   "\x1b[32m"
#define COLOR_START_ORANGE  "\x1b[33m"
#define COLOR_START_RED     "\x1b[31m"

#define COLOR_RESET         "\x1b[0m"

#define X_COLOR(color_start, fmt) color_start fmt COLOR_RESET

#define COLOR_GREEN(fmt)    X_COLOR(COLOR_START_GREEN, fmt)
#define COLOR_ORANGE(fmt)   X_COLOR(COLOR_START_ORANGE, fmt)
#define COLOR_RED(fmt)      X_COLOR(COLOR_START_RED, fmt)

#else

#define COLOR_GREEN(fmt)    fmt
#define COLOR_ORANGE(fmt)   fmt
#define COLOR_RED(fmt)      fmt

#endif /* HYVEMIND_USE_ANSI_COLORING */

#ifdef HYVEMIND_DEBUG_BUILD

#define pr_debug(fmt, ...)  printf(fmt, __VA_ARGS__)
#define pr_info(fmt, ...)   printf(COLOR_GREEN(fmt), __VA_ARGS__)
#define pr_warn(fmt, ...)   printf(COLOR_ORANGE(fmt), __VA_ARGS__)
#define pr_error(fmt, ...)  printf(COLOR_RED(fmt), __VA_ARGS__)

#else

#define pr_debug(fmt, ...)  do {} while (0)
#define pr_info(fmt, ...)   do {} while (0)
#define pr_warn(fmt, ...)   do {} while (0)
#define pr_error(fmt, ...)  do {} while (0)

#endif /* HYVEMIND_DEBUG_BUILD */

/**
 * See docs/printf for a list & description of accepted format specifiers.
 */
#define printf(fmt, ...) __printf(PRINT_PREFIX fmt "\n" __VA_OPT__(,) __VA_ARGS__)
int __printf(const char *fmt, ...);

#endif /* _HYVEMIND_PRINTF_H */

