#ifndef _HYVEMIND_CPU_TOPOLOGY_H
#define _HYVEMIND_CPU_TOPOLOGY_H

#include "types.h"
#include "limine/limine.h"

/* Mask of all present CPUs */
extern uint32_t cpumask_present;
/* Mask of all online CPUs */
extern uint32_t cpumask;

void map_cpu_topology(const struct limine_mp_response *mp_info);

#endif /* _HYVEMIND_CPU_TOPOLOGY_H */

