#ifndef _HYVEMIND_X64_ASM_PGTABLE_TYPES_H
#define _HYVEMIND_X64_ASM_PGTABLE_TYPES_H

#include "mm_types.h"
#include <stdint.h>
#include <stdbool.h>

#define PML4_ENTRY_RANGE_BYTES    (U64(1)<<39)
#define PDPT_ENTRY_RANGE_BYTES    (U64(1)<<30)
#define PD_ENTRY_RANGE_BYTES      (U64(1)<<21)
#define PT_ENTRY_RANGE_BYTES      (U64(1)<<12)

static inline int
pml4_entry_index(const virt_addr_t addr)
{
    return (addr << 16) >> 55;
}

static inline int
pdpt_entry_index(const virt_addr_t addr)
{
    return (addr << 25) >> 55;
}

static inline int
pdt_entry_index(const virt_addr_t addr)
{
    return (addr << 34) >> 55;
}

static inline int
pt_entry_index(const virt_addr_t addr)
{
    return (addr << 43) >> 55;
}

/* CR3 interpretation with 4-&5-lvl paging */
struct cr3 {
    uint64_t ignored0       : 3,
             pwt            : 1,
             pcd            : 1,
             ignored1       : 7,
             paddr          : 40,
             reserved0      : 9,
             lam57          : 1,
             lam48          : 1,
             reserved1      : 1;
} __attribute__((packed));

#define PAGING_STRUCTURE_ENTRIES 512
struct pgtable_structure {
    uint64_t entries[PAGING_STRUCTURE_ENTRIES];
};
typedef struct pgtable_structure pgtable_structure_t;

typedef pgtable_structure_t pml4_t;
typedef pgtable_structure_t pdpt_t;
typedef pgtable_structure_t pd_t;
typedef pgtable_structure_t pt_t;

struct pgtable_entry {
    uint64_t present    : 1,
             ignored0   : 6,
             ps         : 1,
             ignored1   : 56;
} __attribute__((packed));
typedef struct pgtable_entry pgtable_entry_t;

/**
 * TODO: only need two structs, one for entries that map a page and one for
 * entries that don't. Give generic names so they can be reused properly
 */

struct pml4_entry {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             ignored0   : 1,
             ps         : 1,
             ignored1   : 3,
             r          : 1,
             paddr      : 40,
             ignored3   : 11,
             xd         : 1;
} __attribute__((packed));
typedef struct pml4_entry pml4e_t;

/* pdpt entry referencing a page directory */
struct pdpt_entry_no_ps {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             ignored0   : 1,
             ps         : 1,
             ignored1   : 3,
             r          : 1,
             paddr      : 40,
             ignored3   : 11,
             xd         : 1;
} __attribute__((packed));

/* pdpt entry that maps a 1gb page */
struct pdpt_entry_ps {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             d          : 1,
             ps         : 1,
             g          : 1,
             ignored0   : 2,
             r          : 1,
             pat        : 1,
             reserved0  : 17,
             paddr      : 22,
             ignored2   : 7,
             pkey       : 4,
             xd         : 1;
} __attribute__((packed));

union pdpt_entry {
    pgtable_entry_t generic_entry;
    struct pdpt_entry_no_ps entry_no_ps;
    struct pdpt_entry_ps entry_ps;
};
typedef union pdpt_entry pdpt_entry_t;

struct pdt_entry_ps {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             d          : 1,
             ps         : 1,
             g          : 1,
             ignored0   : 2,
             r          : 1,
             pat        : 1,
             reserved0  : 8,
             paddr      : 31,
             ignored1   : 7,
             pkey       : 4,
             xd         : 1;
} __attribute__((packed));

struct pdt_entry_no_ps {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             ignored0   : 1,
             ps         : 1,
             ignored1   : 3,
             r          : 1,
             paddr      : 40,
             ignored2   : 11,
             xd         : 1;
} __attribute__((packed));

union pdt_entry {
    pgtable_entry_t generic_entry;
    struct pdt_entry_no_ps entry_no_ps;
    struct pdt_entry_ps entry_ps;
};
typedef union pdt_entry pdt_entry_t;

struct pt_entry {
    uint64_t present    : 1,
             rw         : 1,
             us         : 1,
             pwt        : 1,
             pcd        : 1,
             a          : 1,
             d          : 1,
             pat        : 1,
             g          : 1,
             ignored0   : 2,
             r          : 1,
             paddr      : 40,
             ignored1   : 7,
             pkey       : 4,
             xd         : 1;
} __attribute__((packed));
typedef struct pt_entry pt_entry_t;

static inline bool
pgtable_entry_present(const void *entry)
{
    return ((pgtable_entry_t *)entry)->present;
}

static inline bool
pgtable_entry_maps_page(const void *entry)
{
    return ((pgtable_entry_t *) entry)->ps;
}

#endif /* _HYVEMIND_X64_ASM_PGTABLE_TYPES_H */

