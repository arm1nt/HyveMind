#ifndef _HYVEMIND_FATAL_H
#define _HYVEMIND_FATAL_H

#include "printf.h"
#include "types.h"

#define __no_return __attribute__((noreturn))

#define die() __die(__FILE__, __LINE__)
#define die_reason(msg) __die_reason(__FILE__, __LINE__, (msg))

static inline void __no_return
__die(const char *file, const int line)
{
    printf("Termination initiated from '%s:%lu'", file, U64(line));

    for (;;) {
        asm volatile ("hlt");
    }
}

static inline void __no_return
__die_reason(const char *file, const int line, const char *msg)
{
    printf("%s", msg);
    __die(file, line);
}

#endif /* _HYVEMIND_FATAL_H */

