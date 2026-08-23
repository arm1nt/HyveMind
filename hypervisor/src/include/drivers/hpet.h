#ifndef _HYVEMIND_DRIVERS_HPET_H
#define _HYVEMIND_DRIVERS_HPET_H

#include "stdint.h"

enum hpet_status {
    HPET_SUCCESS,
    HPET_NOT_SUPPORTED,
    HPET_ERROR
};

int init_hpet(void);

void hpet_do_busy_sleep(const uint64_t time_ns);

#endif /* _HYVEMIND_DRIVERS_HPET_H */

