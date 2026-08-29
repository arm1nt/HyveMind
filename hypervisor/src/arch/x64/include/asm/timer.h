#ifndef _HYVEMIND_X64_ASM_TIMER_H
#define _HYVEMIND_X64_ASM_TIMER_H

#include "../../../include/timer.h"

int arch_init_timer_framework(void);

uint64_t arch_get_timer_frequency(void);

int arch_reprogram_timer(const uint64_t deadline);

#endif /* _HYVEMIND_X64_ASM_TIMER_H */

