#ifndef _HYVEMIND_VM_H
#define _HYVEMIND_VM_H

#include "sched.h"
#include "spinlock.h"
#include "vm_config.h"
#include "asm/vm.h"
#include "lib/list.h"

enum vcpu_state {
    VCPU_CREATED,
    VCPU_RUNNING,
    VCPU_NOT_RUNNING,
    VCPU_READY,
    VCPU_NOT_READY,
};

struct vcpu {
    struct vm *vm;
    spinlock_t lock;
    enum vcpu_state state;

    struct sched_unit sched_unit;

    struct arch_vcpu arch;
};
typedef struct vcpu vcpu_t;

struct vm_physical_memory {
    phys_addr_t start;
    phys_addr_t end;
};

struct vm {
    char *name;
    struct list_head vm_list;

    unsigned int nr_vcpus;
    vcpu_t **vcpus;

    struct vm_physical_memory phys_mem;
    struct arch_vm arch_vm;
};

struct vm * create_vm(const struct vm_config *config);
void destroy_vm(struct vm *vm);

void terminate_vm(struct vm *vm);

uint64_t get_vm_memory_size(const struct vm *vm);
bool vm_memory_contig_range_fits(
        const struct vm *vm,
        const gpaddr start,
        const uint64_t size
);
int copy_to_vm_gpaddr(
        struct vm *vm,
        const gpaddr start,
        const void *data,
        const uint64_t size
);

#endif /* _HYVEMIND_VM_H */

