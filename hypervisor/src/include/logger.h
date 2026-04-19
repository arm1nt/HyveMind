#ifndef _HYVEMIND_LOGGER_H
#define _HYVEMIND_LOGGER_H

#include <stdarg.h>

int init_debug_logging(void);

void debug_printf(const char *fmt, ...);

#endif /* _HYVEMIND_LOGGER_H */

