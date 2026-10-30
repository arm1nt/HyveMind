#ifndef _HYVEMIND_X64_VM_VPIC_DEFS_H
#define _HYVEMIND_X64_VM_VPIC_DEFS_H

#include "types.h"

#define A0_SET(port)    IS_SET(port, U8(1))
#define A0_CLEAR(port)  IS_CLEAR(port, U8(1))

#define D0_MASK U8(1)
#define D1_MASK U8_LSHIFT(1, 1)
#define D2_MASK U8_LSHIFT(1, 2)
#define D3_MASK U8_LSHIFT(1, 3)
#define D4_MASK U8_LSHIFT(1, 4)
#define D5_MASK U8_LSHIFT(1, 5)
#define D6_MASK U8_LSHIFT(1, 6)
#define D7_MASK U8_LSHIFT(1, 7)

/**
 * The command type is fully determined by the PIC's initialization state and
 * by A0, D3 & D4:
 *  - ICW1: A0=0 and D4=1
 *  Outside an init sequence, OCWs can be received:
 *  - OCW1: A0=1
 *  - OCW2: A0=0 and D4=0 and D3=0
 *  - OCW3: A0=0 and D4=0 and D3=1
 */
#define IS_ICW1(port, val) (A0_CLEAR(port) && IS_SET(val, D4_MASK))
#define IS_OCW1(port, val) A0_SET(port)
#define IS_OCW2(port, val) (A0_CLEAR(port) && IS_CLEAR(val, D4_MASK) && IS_CLEAR(val, D3_MASK))
#define IS_OCW3(port, val) (A0_CLEAR(port) && IS_CLEAR(val, D4_MASK) && IS_SET(val, D3_MASK))

#define ICW1_IC4    D0_MASK
#define ICW1_SNGL   D1_MASK
#define ICW1_SETS(val, mask) IS_SET(val, mask)

#define ICW4_UPM    D0_MASK
#define ICW4_AEOI   D1_MASK
#define ICW4_MS     D2_MASK
#define ICW4_BUF    D3_MASK
#define ICW4_SFNM   D4_MASK

#define OCW2_L0     D0_MASK
#define OCW2_L1     D1_MASK
#define OCW2_L2     D2_MASK
#define OCW2_EOI    D5_MASK
#define OCW2_SL     D6_MASK
#define OCW2_R      D7_MASK
#define OCW2_SETS(val, mask) IS_SET(val, mask)

#define OCW3_RIS    D0_MASK
#define OCW3_RR     D1_MASK
#define OCW3_P      D2_MASK
#define OCW3_SMM    D5_MASK
#define OCW3_ESMM   D6_MASK
#define OCW3_SETS(val, mask) IS_SET(val, mask)

/**
 * We restrict the implementation to not support:
 *  - buffered mode
 *  - special fully nested mode
 *  - special mask mode
 *  - automatic EOIs
 *  - specific EOIs
 *  - automatic priority rotation
 *  - manually setting priority
 *  - poll cmd
 */
#define X_UNSUPPORTED_FEATURES(val, req0, req1) \
    (((val) & req0) || (((val) & req1) != req1))

#define ICW1_REQ0 0
#define ICW1_REQ1 0
#define ICW1_UNSUPPORTED(val) X_UNSUPPORTED_FEATURES(val, ICW1_REQ0, ICW1_REQ1)

#define ICW4_REQ0 (ICW4_AEOI | ICW4_BUF | ICW4_SFNM)
#define ICW4_REQ1 0
#define ICW4_UNSUPPORTED(val) X_UNSUPPORTED_FEATURES(val, ICW4_REQ0, ICW4_REQ1)

#define OCW2_REQ0 (OCW2_SL | OCW2_R | OCW2_L0 | OCW2_L1 | OCW2_L2)
#define OCW2_REQ1 (OCW2_EOI)
#define OCW2_UNSUPPORTED(val) X_UNSUPPORTED_FEATURES(val, OCW2_REQ0, OCW2_REQ1)

#define OCW3_REQ0 (OCW3_ESMM | OCW3_SMM | OCW3_P)
#define OCW3_REQ1 0
#define OCW3_UNSUPPORTED(val) X_UNSUPPORTED_FEATURES(val, OCW3_REQ0, OCW3_REQ1)

#endif /* _HYVEMIND_X64_VM_VPIC_DEFS_H */

