#ifndef _HYVEMIND_INTERNAL_HALLOC_H
#define _HYVEMIND_INTERNAL_HALLOC_H

#include "types.h"
#include <stdint.h>

/**
 * [ b7 b6 b5 b4 b3 b2 b1 b0 ]
 *
 * - b2b1b0 ... bin index
 * - b3     ... reserved to 0
 * - b4     ... page granularity allocation flag (yes if set, no otherwise)
 * - b5     ... page managed by halloc flag (yes if set, no otherwise)
 */
typedef uint8_t halloc_flags_t;

#define HALLOC_FLAGS_CLEAR              0
#define HALLOC_FLAGS_PAGE_GRANULARITY   (U8(1) << 4)
#define HALLOC_FLAGS_MANAGED            (U8(1) << 5)
#define HALLOC_FLAGS_BIN_INDEX_MASK     (U8(7))


#define NR_OF_BINS 7

#define BIN_32B_INDEX       0
#define BIN_64B_INDEX       1
#define BIN_128B_INDEX      2
#define BIN_256B_INDEX      3
#define BIN_512B_INDEX      4
#define BIN_1024B_INDEX     5
#define BIN_2048B_INDEX     6
#define INVALID_BIN_INDEX   10

#define BIN_32B_SHIFT       5
#define BIN_64B_SHIFT       6
#define BIN_128B_SHIFT      7
#define BIN_256B_SHIFT      8
#define BIN_512B_SHIFT      9
#define BIN_1024B_SHIFT     10
#define BIN_2048B_SHIFT     11

#define BIN_32B_NR_INIT_PAGES           1
#define BIN_64B_NR_INIT_PAGES           1
#define BIN_128B_NR_INIT_PAGES          1
#define BIN_256B_NR_INIT_PAGES          1
#define BIN_512B_NR_INIT_PAGES          1
#define BIN_1024B_NR_INIT_PAGES         4
#define BIN_2048B_NR_INIT_PAGES         8

#define BIN_32B_REPOPULATE_NR_PAGES     1
#define BIN_64B_REPOPULATE_NR_PAGES     1
#define BIN_128B_REPOPULATE_NR_PAGES    1
#define BIN_256B_REPOPULATE_NR_PAGES    1
#define BIN_512B_REPOPULATE_NR_PAGES    2
#define BIN_1024B_REPOPULATE_NR_PAGES   4
#define BIN_2048B_REPOPULATE_NR_PAGES   8

struct bin_config {
    uint8_t bin_index;
    int chunk_shift;
    int nr_of_init_pages;
    int repopulate_nr_pages;
};

static struct bin_config bin_configs[NR_OF_BINS] = {
    {
        .bin_index = BIN_32B_INDEX,
        .chunk_shift= BIN_32B_SHIFT,
        .nr_of_init_pages = BIN_32B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_32B_REPOPULATE_NR_PAGES,
    },
    {
        .bin_index = BIN_64B_INDEX,
        .chunk_shift= BIN_64B_SHIFT,
        .nr_of_init_pages = BIN_64B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_64B_REPOPULATE_NR_PAGES,
    },
    {
        .bin_index = BIN_128B_INDEX,
        .chunk_shift= BIN_128B_SHIFT,
        .nr_of_init_pages = BIN_128B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_128B_REPOPULATE_NR_PAGES,
    },
    {
        .bin_index = BIN_256B_INDEX,
        .chunk_shift= BIN_256B_SHIFT,
        .nr_of_init_pages = BIN_256B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_256B_REPOPULATE_NR_PAGES
    },
    {
        .bin_index = BIN_512B_INDEX,
        .chunk_shift= BIN_512B_INDEX,
        .nr_of_init_pages = BIN_512B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_512B_REPOPULATE_NR_PAGES,
    },
    {
        .bin_index = BIN_1024B_INDEX,
        .chunk_shift= BIN_1024B_SHIFT,
        .nr_of_init_pages = BIN_1024B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_1024B_REPOPULATE_NR_PAGES,
    },
    {
        .bin_index = BIN_2048B_INDEX,
        .chunk_shift= BIN_2048B_SHIFT,
        .nr_of_init_pages = BIN_2048B_NR_INIT_PAGES,
        .repopulate_nr_pages = BIN_2048B_REPOPULATE_NR_PAGES,
    },
};

#endif /* _HYVEMIND_INTERNAL_HALLOC_H */

