#ifndef _HYVEMIND_VM_H
#define _HYVEMIND_VM_H

#include "guest_config.h"
#include "asm/vm.h"

struct vcpu {
    struct vm *vm;
    struct arch_vcpu arch;
};
typedef struct vcpu vcpu_t;

struct vm {
    char *name;

    unsigned int nr_vcpus;
    vcpu_t **vcpus;
};

struct vm * create_vm(const struct guest_config *config);
void destroy_vm(struct vm *vm);

#endif /* _HYVEMIND_VM_H */

