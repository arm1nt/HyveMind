#ifndef _HYVEMIND_X64_ASM_PGTABLE_TYPES_H
#define _HYVEMIND_X64_ASM_PGTABLE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define PML4_ENTRY_RANGE_BYTES      (U64(1)<<39)
#define PDPT_ENTRY_RANGE_BYTES      (U64(1)<<30)
#define PDT_ENTRY_RANGE_BYTES       (U64(1)<<21)
#define PT_ENTRY_RANGE_BYTES        (U64(1)<<12)

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

/* Page table entry that maps a page */
struct pgtable_entry_ps {
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
             paddr      : 39,
             reserved0  : 7,
             pkey       : 4,
             xd         : 1;
} __attribute__((packed));

/* Page table entry that references another paging structure */
struct pgtable_entry_nops {
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

/**
 * The page table entry struct differs slightly from the structure of
 * entries mapping 1gb or 2mb pages.
 */
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

typedef struct pgtable_entry_nops pml4e_t;
typedef struct pgtable_entry_nops pdpte_nops_t;
typedef struct pgtable_entry_ps pdpte_ps_t;
typedef struct pgtable_entry_nops pdt_nops_t;
typedef struct pgtable_entry_ps pdt_ps_t;

union pgtable_entry {
    uint64_t raw_entry;
    struct pgtable_entry_ps entry_ps;
    struct pgtable_entry_nops entry_nops;
    struct pt_entry pt_entry;
};
typedef union pgtable_entry pgtable_entry_t;

#define PGTABLE_PRESENT         (U64(1))
#define PGTABLE_RW              (U64(2))
#define PGTABLE_PWT             (U64(1) << 3)
#define PGTABLE_PCD             (U64(1) << 4)
#define PGTABLE_XD              (U64(1) << 63)

#endif /* _HYVEMIND_X64_ASM_PGTABLE_TYPES_H */

