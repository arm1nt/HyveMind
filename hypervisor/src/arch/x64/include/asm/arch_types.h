#ifndef _HYVEMIND_X64_ASM_ARCH_TYPES_H
#define _HYVEMIND_X64_ASM_ARCH_TYPES_H

#include <stdint.h>

typedef uint32_t cpuid_t;
typedef uint32_t lapicid_t;

#define INVALID_PROCESSOR_ID (~((cpuid_t) 0))
#define INVALID_LAPIC_ID (~((lapicid_t) 0))

#endif /* _HYVEMIND_X64_ASM_ARCH_TYPES_H */

