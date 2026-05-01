#ifndef _HYVEMIND_FATAL_H
#define _HYVEMIND_FATAL_H

#include "printf.h"

#define __no_return __attribute__((noreturn))

#define die() __die(__FILE__, __LINE__)

static void __no_return
__die(const char *file, const int line)
{
    printf("Termination initiated from '%s:TODO'", file);

    for (;;) {
        asm volatile ("hlt");
    }
}

#endif /* _HYVEMIND_FATAL_H */

