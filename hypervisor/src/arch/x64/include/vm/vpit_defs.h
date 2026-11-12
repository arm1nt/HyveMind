#ifndef _HYVEMIND_X64_VM_VPIT_DEFS_H
#define _HYVEMIND_X64_VM_VPIT_DEFS_H

#include "types.h"

#define PORT_SELECTOR_MASK 0x03
#define COUNTER1_PORT   0
#define COUNTER2_PORT   1
#define COUNTER3_PORT   2
#define CWR_PORT        3

#define CWR_MODE_VAL_SHIFT  1
#define CWR_MODE_VAL_MASK   0x07
#define CWR_MODE0_VAL   0
#define CWR_MODE1_VAL   1
#define CWR_MODE2_VAL1  2
#define CWR_MODE2_VAL2  6
#define CWR_MODE3_VAL1  3
#define CWR_MODE3_VAL2  7
#define CWR_MODE4_VAL   4
#define CWR_MODE5_VAL   5

#define CWR_RW_VAL_SHIFT    4
#define CWR_RW_VAL_MASK     0x03
#define CWR_RW_LSB      1
#define CWR_RW_MSB      2
#define CWR_RW_LSB_MSB  3

#define CWR_SC_VAL_SHIFT    6
#define CWR_SC_VAL_MASK     0x03
#define CWR_SC_COUNTER0_VAL 0
#define CWR_SC_COUNTER1_VAL 1
#define CWR_SC_COUNTER2_VAL 2

#define CWR_BCD_VAL 1

#define IS_READ_BACK_CMD(val) (IS_SET(val, 1 << 7) && IS_SET(val, 1 << 6))
#define IS_COUNTER_LATCH_CMD(val) (IS_CLEAR(val, 1 << 5) && IS_CLEAR(val, 1 << 4))

#define VPIT_FREQUENCY U64(1193182) /* hz */
#define VPIT_MIN_PERIOD 100000 /* ns */

#define VPIT_IRQ_NR 0x00

#endif /* _HYVEMIND_X64_VM_VPIT_DEFS_H */

