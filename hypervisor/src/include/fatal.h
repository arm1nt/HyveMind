#ifndef _HYVEMIND_FATAL_H
#define _HYVEMIND_FATAL_H

#define __no_return __attribute__((noreturn))

static void __no_return
die(void)
{
    for (;;) {
        asm volatile ("hlt");
    }
}

#endif /* _HYVEMIND_FATAL_H */

