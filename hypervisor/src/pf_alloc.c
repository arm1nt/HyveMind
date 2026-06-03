#include "limine/helpers.h"
#include "printf.h"
#include "pf_alloc.h"
#include "phys_mm.h"
#include "string.h"
#include "asm/paging.h"

#include <stddef.h>
#include <stdbool.h>

/**
 * todo: replace with / add a binary-buddy allocator that maintains an array of
 * page structs and move the page struct array out of halloc.
 */

#undef PRINT_PREFIX_NAME
#define PRINT_PREFIX_NAME "pf-allocator"

struct pf_bitmap_allocator {
    uint8_t *bitmap;
    uint64_t bitmap_size;

    /* Index in the bitmap right after the most recent allocation */
    uint64_t last_alloc_byte_hint;
    uint8_t last_alloc_bit_hint;

    uint64_t allocatable_frames;
    uint64_t allocated_page_frames;
};

static struct pf_bitmap_allocator *pf_allocator = NULL;

/* todo: move this printing logic somewhere else */
static void
print_limine_memory_map(const struct limine_memmap_response *mem_map)
{
    uint64_t counter = 0;
    struct limine_memmap_entry *entry;

    while (counter < mem_map->entry_count) {
        entry = mem_map->entries[counter];

        printf(
                "[%s]\t\t %lx - %lx \t (len: %lu)",
                limine_mem_type_to_string[entry->type],
                entry->base,
                entry->base + entry->length,
                entry->length
        );

        counter++;
    }
}

struct alloc_init_info {
    const struct limine_memmap_response *mem_map;
    const uint64_t direct_mapping_offset;
};

static int
find_contiguous_memory_region(
        const struct alloc_init_info *init_info,
        const uint64_t req_region_size,
        virt_addr_t __directly_mapped *region_start_addr
)
{
    /* Currently, we simply take the first usable region that fits */

    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < init_info->mem_map->entry_count; i++) {
        entry = init_info->mem_map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        if (entry->length > req_region_size) {
            /* Its guaranteed by the bootloader that this address is page-aligned */
            *region_start_addr = __phys_to_virt(
                    entry->base,
                    init_info->direct_mapping_offset
            );
            return 0;
        }
    }

    return -1;
}

static inline void
init_pf_allocator_metadata(
        const virt_addr_t __directly_mapped allocator_region,
        const uint64_t bitmap_length
)
{
    pf_allocator = (struct pf_bitmap_allocator *) allocator_region;
    memset(pf_allocator, 0, sizeof(struct pf_bitmap_allocator));

    pf_allocator->bitmap_size = bitmap_length;
    pf_allocator->bitmap = (uint8_t *) (allocator_region + sizeof(struct pf_bitmap_allocator));
    memset(pf_allocator->bitmap, 0xFF, bitmap_length);

    pf_allocator->allocatable_frames = 0;
}

static inline void
allocated_nr_frames(const uint64_t nr)
{
    pf_allocator->allocated_page_frames += nr;
    pf_allocator->allocatable_frames -= nr;
}

static inline void
freed_nr_frames(const uint64_t nr)
{
    pf_allocator->allocatable_frames += nr;
    pf_allocator->allocated_page_frames -= nr;
}

static inline void
set_bitmap_entry(const phys_addr_t addr)
{
    const uint64_t pfn = phys_to_pfn(addr);
    const uint64_t byte_index = pfn >> 3;
    const uint64_t bit_index = pfn % 8;

    uint8_t *byte = &pf_allocator->bitmap[byte_index];
    *byte = *byte | U8_RSHIFT(128, bit_index);
}

static inline void
clear_bitmap_entry(const phys_addr_t addr)
{

    const uint64_t pfn = phys_to_pfn(addr);
    const uint64_t byte_index = pfn >> 3;
    const uint64_t bit_index = pfn % 8;

    uint8_t *byte = &pf_allocator->bitmap[byte_index];
    *byte = *byte & (~U8_RSHIFT(128, bit_index));
}

static inline void
set_bitmap_region(const phys_addr_t start_addr, const uint64_t nr)
{
    for (uint64_t i = 0; i < nr; i++) {
        set_bitmap_entry(start_addr + (i * PAGE_SIZE));
    }
}

static inline void
clear_bitmap_region(const phys_addr_t start_addr, const uint64_t nr)
{
    for (uint64_t i = 0; i < nr; i++) {
        clear_bitmap_entry(start_addr + (i * PAGE_SIZE));
    }
}

/* Computes how many pages are needed to hold 'len' bytes */
#define BYTE_LEN_TO_PAGES(len) ((len / PAGE_SIZE) + (len % PAGE_SIZE != 0))

static void
early_initialize_pf_bitmap(const struct alloc_init_info *init_info)
{
    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < init_info->mem_map->entry_count; i++) {
        entry = init_info->mem_map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        /**
         * If the region base is not page aligned and/or the region's length is
         * not a multiple of page-size, we mark more than the region as used.
         * This is fine since usable and bootloader-reclaimable regions are
         * page-aligned & not overlapping with such regions, and will therefore
         * not be affected.
         */
        clear_bitmap_region(entry->base, BYTE_LEN_TO_PAGES(entry->length));
        pf_allocator->allocatable_frames += BYTE_LEN_TO_PAGES(entry->length);
    }

    /* Mark the page frames used by the allocator's metadata as used */
    const phys_addr_t allocator_base = __virt_to_phys(
            (uint64_t) pf_allocator,
            init_info->direct_mapping_offset
    );
    const uint64_t allocator_size = sizeof(struct pf_bitmap_allocator) + pf_allocator->bitmap_size;
    const uint64_t allocator_used_pages = BYTE_LEN_TO_PAGES(allocator_size);
    set_bitmap_region(allocator_base, allocator_used_pages);
    allocated_nr_frames(allocator_used_pages);
}

int
early_init_page_frame_allocator(
        const struct limine_memmap_response *mem_map,
        const uint64_t direct_mapping_offset
)
{
    print_limine_memory_map(mem_map);

    const sys_mem_info_t *mem_info = get_system_memory_info();
    const struct alloc_init_info init_info = {
        .mem_map = mem_map,
        .direct_mapping_offset = direct_mapping_offset
    };

    const uint64_t nr_of_pfns = mem_info->nr_of_pfns;
    /* Nr of bytes needed so that we can fit 1 bit per pfn */
    const uint64_t bitmap_size = (nr_of_pfns >> 3) + (nr_of_pfns % 8 != 0);
    const uint64_t allocator_region_size = sizeof(struct pf_bitmap_allocator) + bitmap_size;

    virt_addr_t __directly_mapped pf_allocator_region;
    int ret = find_contiguous_memory_region(
            &init_info,
            allocator_region_size,
            &pf_allocator_region
    );

    if (ret != 0) {
        printf("No large-enough contiguous region of memory available for storing allocator metadata");
        return -1;
    }

    init_pf_allocator_metadata(pf_allocator_region, bitmap_size);

    early_initialize_pf_bitmap(&init_info);

    return 0;
}

static int
__get_pages(const uint64_t nr, phys_addr_t *start_addr)
{
    const uint64_t free_frames =
        pf_allocator->allocatable_frames - pf_allocator->allocated_page_frames;

    if (nr == 0 || nr > free_frames) {
        return -1;
    }

    uint8_t bit_pos = pf_allocator->last_alloc_bit_hint;
    uint64_t byte_pos = pf_allocator->last_alloc_byte_hint;
    uint64_t free_contig_area_start = (byte_pos << 3) + bit_pos;

    const uint64_t total_bits = pf_allocator->bitmap_size << 3;
    uint64_t explored_bits = 0;
    bool free_streak = false;
    uint64_t remaining_needed = nr;

    /* If we are currently on a free streak, allow looking at bitmap positions twice */
    while (((explored_bits < total_bits) || free_streak) && remaining_needed) {
        uint8_t entry = pf_allocator->bitmap[byte_pos];
        uint8_t is_used = entry & U8_RSHIFT(128, bit_pos);

        if (!is_used) {
            if (remaining_needed == nr) {
                free_contig_area_start = (byte_pos << 3) + bit_pos;
            }

            free_streak = true;
            remaining_needed--;

            if (!remaining_needed) {
                goto out_found_area;
            }

        } else {
            remaining_needed = nr;
            free_streak = false;
        }

        explored_bits++;
        bit_pos = (bit_pos + 1) % 8;
        byte_pos = (bit_pos == 0)
            ? (byte_pos + 1) % pf_allocator->bitmap_size
            : byte_pos;

        if (byte_pos == 0 && bit_pos == 0) {
            /* Wrap around occurs - break off any contigous streak */
            remaining_needed = nr;
            free_streak = false;
        }
    }

    return -1;

out_found_area:
    ;
    const uint8_t area_bit_index = free_contig_area_start % 8;
    const uint64_t area_byte_index = free_contig_area_start >> 3;
    const uint64_t pfn = (area_byte_index << 3) + area_bit_index;

    set_bitmap_region(pfn_to_phys(pfn), nr);
    allocated_nr_frames(nr);

    *start_addr =pfn_to_phys(pfn);

    /* Store hint for the next allocation */
    pf_allocator->last_alloc_bit_hint = (area_bit_index + (nr%8)) % 8;

    const uint64_t byte_hint_offset = (nr / 8) + (nr%8 == 0 ? 1 : 0);
    if (pf_allocator->last_alloc_bit_hint == 0) {
        pf_allocator->last_alloc_byte_hint =
            (area_byte_index + 1 + byte_hint_offset) % pf_allocator->bitmap_size;
    } else {
        pf_allocator->last_alloc_byte_hint =
            (area_byte_index + byte_hint_offset) % pf_allocator->bitmap_size;
    }

    return 0;
}

inline int
get_page_raw(phys_addr_t *pf_addr)
{
    return __get_pages(1, pf_addr);
}

inline int
get_pages_raw(const uint64_t nr, phys_addr_t *start_pf_addr)
{
    return __get_pages(nr, start_pf_addr);
}

inline int
get_page(virt_addr_t __directly_mapped *addr)
{
    return get_pages(1, addr);
}

int
get_page_zeroed(virt_addr_t __directly_mapped *addr)
{
    int ret = get_page(addr);
    if (ret != 0) {
        return ret;
    }

    memset((void *) *addr, 0, PAGE_SIZE);
    return 0;
}

inline int
get_pages(const uint64_t nr, virt_addr_t __directly_mapped *start_addr)
{
    phys_addr_t phys_addr;

    if (__get_pages(nr, &phys_addr) != 0) {
        return -1;
    }

    *start_addr = phys_to_virt(phys_addr);
    return 0;
}

int
get_pages_zeroed(const uint64_t nr, virt_addr_t __directly_mapped *start_addr)
{
    int ret = get_pages(nr, start_addr);
    if (ret != 0) {
        return ret;
    }

    memset((void *) *start_addr, 0, nr << PAGE_SHIFT);
    return 0;
}

inline int
free_page_raw(const phys_addr_t addr)
{
    return free_pages_raw(1, addr);
}

inline int
free_pages_raw(const uint64_t nr, const phys_addr_t addr)
{
    clear_bitmap_region(addr, nr);
    freed_nr_frames(nr);
    return 0;
}

inline int
free_page(const virt_addr_t __directly_mapped addr)
{
    return free_page_raw(virt_to_phys(addr));
}

inline int
free_pages(const uint64_t nr, const virt_addr_t __directly_mapped addr)
{
    return free_pages_raw(nr, virt_to_phys(addr));
}

