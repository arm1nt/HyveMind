#include "halloc.h"
#include "printf.h"
#include "string.h"
#include "vm.h"

static void
launch_vcpu(struct sched_unit *vcpu_sched_unit)
{
    vcpu_t *vcpu = (vcpu_t *) container_of(vcpu_sched_unit, struct vcpu, sched_unit);

    NOT_YET_IMPLEMENTED;
}

static inline int
prepare_vm_execution_env(struct vm *vm, const struct vm_config *config)
{
    return arch_prepare_execution_env(vm, config);
}

static struct vcpu *
allocate_vcpu(void)
{
    struct vcpu *vcpu = (struct vcpu *) hmalloc(sizeof(struct vcpu));
    if (!vcpu)
        return NULL;

    memset(vcpu, 0, sizeof(struct vcpu));

    /* Mark as allocated but not yet initialized and not schedulable */
    vcpu->state = VCPU_CREATED;
    spinlock_init(&vcpu->lock);
    INIT_LIST_HEAD(&vcpu->sched_unit.sched_queue);
    vcpu->sched_unit.ops.launch_task = launch_vcpu;

   if (arch_alloc_vcpu(vcpu) != 0) {
       hfree(vcpu);
       return NULL;
   }

   return vcpu;
}

static inline void
destroy_vcpu(struct vcpu *vcpu)
{
    arch_destroy_vcpu(vcpu);
    hfree(vcpu);
}

static inline void
destroy_vcpus(struct vm *vm, const int nr)
{
    for (int i = 0; i < nr; i++) {
        if (!vm->vcpus[i])
            continue;

        destroy_vcpu(vm->vcpus[i]);
        vm->vcpus[i] = NULL;
    }
}

void
destroy_vm(struct vm *vm)
{
    if (!vm)
        return;

    arch_destroy_vm(vm);
    destroy_vcpus(vm, vm->nr_vcpus);
    hfree(vm->vcpus);
    hfree(vm->name);
    hfree(vm);
}

static struct vm *
allocate_vm(const struct vm_config *config)
{
    struct vm *vm = (struct vm *) hmalloc(sizeof(struct vm));
    if (!vm)
        goto error_out;

    memset(vm, 0, sizeof(struct vm));
    vm->name = strdup_nt(config->name);
    vm->nr_vcpus = config->nr_vcpus;

    vm->vcpus = (vcpu_t **) hmalloc(config->nr_vcpus * sizeof(vcpu_t *));
    if (!vm->vcpus)
        goto error_out_vm_alloc;

    memset(vm->vcpus, 0, config->nr_vcpus * sizeof(vcpu_t *));

    for (int i = 0; i < config->nr_vcpus; i++) {
        vcpu_t *vcpu = allocate_vcpu();
        if (!vcpu) {
            destroy_vcpus(vm, i-1);
            goto error_out_vm_alloc;
        }

        vcpu->vm = vm;
        vm->vcpus[i] = vcpu;
    }

    return vm;

error_out_vm_alloc:
    hfree(vm);
error_out:
    pr_error("Failed to allocate vm structs for %s", config->name);
    return NULL;
}

struct vm *
create_vm(const struct vm_config *config)
{
    struct vm *vm = allocate_vm(config);
    if (!vm)
        return NULL;

    if (prepare_vm_execution_env(vm, config) != 0) {
        destroy_vm(vm);
        return NULL;
    }

    return vm;
}

