#ifndef _HYVEMIND_X64_ASM_TIMER_H
#define _HYVEMIND_X64_ASM_TIMER_H

enum timer_op_status {
    TIMER_SUCCESS,
    TIMER_INIT_FAILED,
    TIMER_ERROR,
};

int init_timer(void);

#endif /* _HYVEMIND_X64_ASM_TIMER_H */

