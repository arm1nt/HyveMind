#ifndef _HYVEMIND_X64_VM_VPIT_H
#define _HYVEMIND_X64_VM_VPIT_H

#include "types.h"
#include "timer.h"

#define vm_vpit(vm) ((vm)->arch_vm.vpit)

enum vpit_counter_id {
    VPIT_COUNTER0,
    VPIT_COUNTER1,
    VPIT_COUNTER2,
};

enum vpit_counter_mode {
    VPIT_COUNTER_MODE0,
    VPIT_COUNTER_MODE1,
    VPIT_COUNTER_MODE2,
    VPIT_COUNTER_MODE3,
    VPIT_COUNTER_MODE4,
    VPIT_COUNTER_MODE5,
};

enum vpit_counter_rw_mode {
    VPIT_RW_LSB,
    VPIT_RW_MSB,
    VPIT_RW_LSB_MSB,
};

#define VPIT_COUNTERS_NR 3

struct vpit_counter {
    enum vpit_counter_id id;
    enum vpit_counter_mode mode;

    struct {
        enum vpit_counter_rw_mode mode;
        bool write_wait_msb;
    } rw;

    union {
        uint16_t full;
        struct {
            uint8_t lsb;
            uint8_t msb;
        };
    } count_register;

    struct timer *timer;
};

struct vpit {
    struct vm *vm;
    struct vpit_counter counters[VPIT_COUNTERS_NR];
};

struct vpit * allocate_vpit(struct vm *vm);
void destroy_vpit(struct vpit *vpit);

void vpit_read(struct vpit *vpit, const uint16_t port, uint8_t *val);
void vpit_write(struct vpit *vpit, const uint16_t port, const uint8_t val);

#endif /* _HYVEMIND_X64_VM_VPIT_H */

