#ifndef _HYVEMIND_TYPES_H
#define _HYVEMIND_TYPES_H

#include <stdint.h>

#define BUILD_LEFT_SHIFT(type, val, shift)  (((type) val) << shift)
#define BUILD_RIGHT_SHIFT(type, val, shift) (((type) val) >> shift)

#define U8_LSHIFT(val, shift) BUILD_LEFT_SHIFT(uint8_t, val, shift)
#define U8_RSHIFT(val, shift) BUILD_RIGHT_SHIFT(uint8_t, val, shift)

#define U64_LSHIFT(val, shift) BUILD_LEFT_SHIFT(uint64_t, val, shift)
#define U64_RSHIFT(val, shift) BUILD_RIGHT_SHIFT(uint64_t, val, shift)

#endif /* _HYVEMIND_TYPES_H */

