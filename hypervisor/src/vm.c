#include "halloc.h"
#include "printf.h"
#include "string.h"
#include "vm.h"

static inline void
destroy_vcpu(vcpu_t *vcpu)
{
    destroy_arch_vcpu(vcpu);
    hfree(vcpu);
}

static inline void
destroy_vcpus(vcpu_t **vcpus, const unsigned int nr)
{
    for (unsigned int i = 0; i < nr; i++) {
        destroy_vcpu(vcpus[i]);
    }
}

inline void
destroy_vm(struct vm *vm)
{
    pr_debug("Destroying VM '%s'", vm->name);
    destroy_vcpus(vm->vcpus, vm->nr_vcpus);
    hfree(vm->vcpus);
    destroy_arch_vm(vm);
    hfree(vm);
}

static inline vcpu_t *
allocate_vcpu(struct vm *vm)
{
    vcpu_t *vcpu = (vcpu_t *) hmalloc(sizeof(vcpu_t));
    if (!vcpu) {
        pr_error("Unable to allocate vcpu struct");
        return NULL;
    }

    memset(vcpu, 0, sizeof(*vcpu));

    vcpu->vm = vm;

    if (allocate_arch_vcpu(vcpu) != 0) {
        pr_error("Unable to allocate arch vcpu members");
        hfree(vcpu);
        return NULL;
    }

    return vcpu;
}

static inline struct vm *
allocate_vm(const struct guest_config *config)
{
    struct vm *vm = (struct vm *) hmalloc(sizeof(struct vm));
    if (!vm) {
        pr_warn("Failed to allocate a 'vm' struct!");
        return NULL;
    }

    memset(vm, 0, sizeof(*vm));

    vm->nr_vcpus = config->nr_vcpus;
    vm->vcpus = (vcpu_t **) hmalloc(sizeof(vcpu_t *) * config->nr_vcpus);
    if (!vm->vcpus) {
        pr_error("Failed to allocate VM's array of vcpu pointers");
        goto error_out_1;
    }

    for (unsigned int i = 0; i < vm->nr_vcpus; i++) {
        vm->vcpus[i] = allocate_vcpu(vm);
        if (!vm->vcpus[i]) {
            pr_error("Failed to allocate %lu-th vcpu struct", U64(i));
            destroy_vcpus(vm->vcpus, MAX(0, i-1));
            goto error_out_2;
        }
    }

    return vm;

error_out_2:
    hfree(vm->vcpus);
error_out_1:
    hfree(vm);
    return NULL;
}

static inline int
init_vm(struct vm *vm, const struct guest_config *config)
{
    vm->name = config->name;
    return arch_init_vm(vm, config);
}

struct vm *
create_vm(const struct guest_config *config)
{
    int ret;

    struct vm *vm = allocate_vm(config);
    if (!vm) {
        pr_error("Unable to allocate a VM instance");
        return NULL;
    }

    ret = init_vm(vm, config);
    if (ret != 0) {
        pr_error("Failed to initialize the allocated VM");
        destroy_vm(vm);
        return NULL;
    }

    pr_debug("Successfully created VM '%s'", vm->name);
    return vm;
}

