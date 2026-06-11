#ifndef _HYVEMIND_PER_CPU_H
#define _HYVEMIND_PER_CPU_H

#ifndef HYVEMIND_MAX_NR_CPUS
    #define HYVEMIND_MAX_NR_CPUS    64
#endif

#define __percpu

#define DECLARE_PER_CPU(_type, _name)                       \
    extern typeof(_type) __percpu_##_name[HYVEMIND_MAX_NR_CPUS]

#define DEFINE_PER_CPU(_type, _name)                        \
    typeof(_type) __percpu_##_name[HYVEMIND_MAX_NR_CPUS]

#define DEFINE_PER_CPU_ALIGNED(_type, _name, _alignment)    \
    DEFINE_PER_CPU(_type, _name) __attribute__((aligned(_alignment)))

#define DEFINE_PER_CPU_VAL(_type, _name, _val)              \
    DEFINE_PER_CPU(_type, _name) = {[0 ... HYVEMIND_MAX_NR_CPUS-1] = (_val) }

extern int get_current_cpuid(void);

#define percpu_ptr(_name) percpu_ptr_raw(_name, get_current_cpuid())
#define percpu_ptr_raw(_name, _cpu) ((void *) &(__percpu_##_name[_cpu]))
#define percpu_val(_name) percpu_val_raw(_name, get_current_cpuid())
#define percpu_val_raw(_name, _cpu) (__percpu_##_name[_cpu])

#define set_percpu_val(_name, _val) set_percpu_val_raw(_name, get_current_cpuid(), _val)
#define set_percpu_val_raw(_name, _cpu, _val) (__percpu_##_name[_cpu] = (_val))

#endif /* _HYVEMIND_PER_CPU_H */

