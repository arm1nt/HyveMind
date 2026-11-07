#ifndef _HYVEMIND_X64_VM_IO_H
#define _HYVEMIND_X64_VM_IO_H

#include "types.h"

struct vcpu;

enum pio_op_direction {
    PIO_OP_READ,
    PIO_OP_WRITE,
};

enum pio_op_size {
    PIO_OP_SIZE_BYTE,
    PIO_OP_SIZE_WORD,
    PIO_OP_SIZE_DWORD,
};

struct pio_operation {
    enum pio_op_direction direction;
    enum pio_op_size size;
    uint16_t port;
};

void handle_guest_io_op(struct pio_operation *pio, struct vcpu *vcpu);

#endif /* _HYVEMIND_X64_VM_IO_H */

