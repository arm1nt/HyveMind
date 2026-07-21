#ifndef _HYVEMIND_VM_H
#define _HYVEMIND_VM_H

#include "guest_config.h"

struct arch_vcpu;
struct arch_vm;

extern void arch_create_vm_from_guest_config(const guest_cfg_t *config);

struct vcpu {
    struct arch_vcpu *arch_vcpu;
};
typedef struct vcpu vcpu_t;

struct vm {
    char *name;
    struct arch_vm *arch_vm;

    unsigned int nr_vcpus;
    vcpu_t **vcpus;
};

static inline void
create_vm_from_guest_config(const guest_cfg_t *config)
{
    arch_create_vm_from_guest_config(config);
}

#endif /* _HYVEMIND_VM_H */

