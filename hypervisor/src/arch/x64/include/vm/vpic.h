#ifndef _HYVEMIND_X64_VM_VPIC_H
#define _HYVEMIND_X64_VM_VPIC_H

#include "types.h"
#include "vm/vpic_defs.h"

struct vm;
#define vm_vpic(x) ((x)->arch_vm.vpic)

enum vpic_state {
    VPIC_UNINITIALIZED,
    VPIC_INITIALIZED,
    VPIC_ICW2,
    VPIC_ICW3,
    VPIC_ICW4,
    VPIC_ICW2_ICW3,
    VPIC_ICW2_ICW4,
    VPIC_ICW2_ICW3_ICW4,
    VPIC_ICW3_ICW4,
};

enum vpic_status_read_mode {
    VPIC_READ_IRR,
    VPIC_READ_ISR,
    VPIC_READ_NO_ACTION,
};

struct __vpic {
    enum vpic_state state;
    uint8_t imr;
    uint8_t irr;
    uint8_t isr;
    uint8_t offset;
    enum vpic_status_read_mode read_mode;
    union {
        /* Each set bit corresponds to a IRQ input behind which is a child vPIC */
        uint8_t sub_device_map;
        /* Set for a sub-device */
        uint8_t sub_device_id;
    };
    struct __vpic *parent_vpic;
};

struct vpic {
    struct __vpic vpic1;
    struct __vpic vpic2;
};

struct vpic *allocate_vpic(void);

void vpic_read(const struct __vpic *vpic, const uint16_t port, uint8_t *val);
void vpic_write(struct __vpic *vpic, const uint16_t port, const uint8_t val);

void print_vpic_state(const struct vpic *vpic);

#endif /* _HYVEMIND_X64_VM_VPIC_H */

