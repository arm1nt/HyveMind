#ifndef _HYVEMIND_TYPES_H
#define _HYVEMIND_TYPES_H

/* TODO: rename to 'bitops.h' or similar */

#include <stdint.h>

#define U8(val)     ((uint8_t)  (val))
#define U16(val)    ((uint16_t) (val))
#define U32(val)    ((uint32_t) (val))
#define U64(val)    ((uint64_t) (val))

#define U64_UPPER32(val)    (U32(val >> 32))
#define U64_LOWER32(val)    (U32((val << 32) >> 32))

#define U64_X_LSBS_SET(nr) (~(~(U64(0))<<nr))

/* Caller is responsible for correct types */
#define SET_BIT(val, mask)      (val | mask)
#define CLEAR_BIT(val, mask)    (val & (~mask))
#define TOGGLE_BIT(val, bit)    (val ^ mask)

#define IS_SET(val, mask)       (val & mask)
#define IS_CLEAR(val, mask)     ((val & mask) == 0)

#define BUILD_LEFT_SHIFT(type, val, shift)  (((type) val) << shift)
#define BUILD_RIGHT_SHIFT(type, val, shift) (((type) val) >> shift)

#define U8_LSHIFT(val, shift) BUILD_LEFT_SHIFT(uint8_t, val, shift)
#define U8_RSHIFT(val, shift) BUILD_RIGHT_SHIFT(uint8_t, val, shift)

#define U8_WRAP_RSHIFT(val) (U8_LSHIFT(val, 7) | U8_RSHIFT(val, 1))

#define U64_LSHIFT(val, shift) BUILD_LEFT_SHIFT(uint64_t, val, shift)
#define U64_RSHIFT(val, shift) BUILD_RIGHT_SHIFT(uint64_t, val, shift)

#endif /* _HYVEMIND_TYPES_H */

