#include "mm.h"
#include "limine/helpers.h"
#include "printf.h"
#include "string.h"

#include <stddef.h>
#include <stdbool.h>

#undef PRINT_PREFIX_NAME
#define PRINT_PREFIX_NAME "pf-allocator"

/* Leave here for now, later on move to paging logic */
uint64_t direct_mapping_offset;

struct pf_bitmap_allocator {
    uint8_t *bitmap;
    uint64_t bitmap_size;

    /* Index in the bitmap right after the most recent allocation */
    uint64_t last_alloc_byte_hint;
    uint8_t last_alloc_bit_hint;

    uint64_t total_usable_page_frames;
    uint64_t allocated_page_frames;
};

/**
 * @total_size ... Total size of existing memory
 * @early_usable_memory ... Total size of early usable memory
 * @total_usable_memory ... Total size of theoretically usable memory
 */
struct system_memory_info {
    uint64_t total_size;
    uint64_t early_usable_memory;
    uint64_t total_usable_memory;

    phys_addr_t lowest_mapped_mem_addr;
    phys_addr_t highest_mapped_mem_addr;
};


static struct system_memory_info sys_mem_info;
static struct pf_bitmap_allocator *pf_allocator = NULL;

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

static int
get_system_memory_info(
        const struct limine_memmap_response *mem_map,
        struct system_memory_info *mem_info
)
{
    uint64_t total_size = 0;
    uint64_t early_usable_size = 0;
    uint64_t total_usable_size = 0;

    phys_addr_t lowest_mapped = ~((phys_addr_t) 0);
    phys_addr_t highest_mapped = 0;

    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

        lowest_mapped = (entry->base < lowest_mapped)
            ? entry->base
            : lowest_mapped;

        highest_mapped = ((entry->base + entry->length -1) > highest_mapped)
            ? (entry->base + entry->length - 1)
            : highest_mapped;

        total_size += entry->length;

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            early_usable_size += entry->length;
            total_usable_size += entry->length;
        } else if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            total_usable_size += entry->length;
        }
    }

    mem_info->total_size = total_size;
    mem_info->early_usable_memory = early_usable_size;
    mem_info->total_usable_memory = total_usable_size;
    mem_info->lowest_mapped_mem_addr = lowest_mapped;
    mem_info->highest_mapped_mem_addr = highest_mapped;

    return 0;
}

static int
find_contiguous_memory_region(
        const struct limine_memmap_response *mem_map,
        const uint64_t req_region_size,
        virt_addr_t __directly_mapped *region_start_addr
)
{
    /* Currently, we simply take the first usable region that fits */

    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        if (entry->length > req_region_size) {
            /* Its guaranteed by the bootloader that this address is page-aligned */
            *region_start_addr = phys_to_directly_mapped(entry->base);
            return 0;
        }
    }

    return -1;
}

static inline void
init_pf_allocator_metadata(
        const virt_addr_t __directly_mapped allocator_region,
        const uint64_t region_size,
        const uint64_t bitmap_length,
        const struct system_memory_info *mem_info
)
{
    pf_allocator = (struct pf_bitmap_allocator *) allocator_region;
    memset(pf_allocator, 0, region_size);

    pf_allocator->bitmap_size = bitmap_length;
    pf_allocator->bitmap = (uint8_t *) (allocator_region + sizeof(struct pf_bitmap_allocator));
    memset(pf_allocator->bitmap, 0xFF, bitmap_length);

    pf_allocator->total_usable_page_frames = mem_info->total_usable_memory / PAGE_SIZE;
}

static inline void
toggle_bitmap_entry(const phys_addr_t addr)
{
    const uint64_t pfn = phys_to_pfn(addr);
    const uint64_t byte_index = pfn >> 3;
    const uint64_t bit_index = pfn % 8;

    uint8_t *byte = &pf_allocator->bitmap[byte_index];
    *byte = *byte ^ U8_RSHIFT(128, bit_index);

    pf_allocator->allocated_page_frames++;
}

/**
 * @nr... nr of page frames to mark as used, starting from the pf corresponding
 *        to the start addr
 */
static inline void
toggle_bitmap_region(const phys_addr_t start_addr, const uint64_t nr)
{
    for (uint64_t i = 0; i < nr; i++) {
        toggle_bitmap_entry(start_addr + (i*PAGE_SIZE));
    }
}

/* Computes how many pages are needed to hold 'len' bytes */
#define BYTE_LEN_TO_PAGES(len) ((len / PAGE_SIZE) + (len % PAGE_SIZE != 0))

static int
early_initialize_pf_bitmap(
        const struct limine_memmap_response *mem_map,
        const uint64_t largest_usable_pfn
)
{
    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

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
        toggle_bitmap_region(entry->base, BYTE_LEN_TO_PAGES(entry->length));
    }

    /* Mark the page frames used by the allocator's metadata as used */
    const phys_addr_t allocator_base = directly_mapped_to_phys((uint64_t)pf_allocator);
    const uint64_t allocator_size = sizeof(struct pf_bitmap_allocator) + pf_allocator->bitmap_size;
    toggle_bitmap_region(allocator_base, BYTE_LEN_TO_PAGES(allocator_size));

    /**
     * Mark bits in the last bitmap byte that do not correspond to valid pfn's,
     * but are there because that byte is necessary to hold entries for every
     * valid pfn, as not-free. (e.g. if we'd have 10 pfs, we'd need 2 bytes)
     */
    const uint8_t invalid_trailing_bitmap_entries = 8 - ((largest_usable_pfn+1) % 8);
    if (invalid_trailing_bitmap_entries < 8) {
        const phys_addr_t invalid_start_addr = pfn_to_phys(largest_usable_pfn+1);
        toggle_bitmap_region(invalid_start_addr, invalid_trailing_bitmap_entries);
    }

    return 0;
}

int
early_init_page_frame_allocator(const struct limine_memmap_response *mem_map)
{
    print_limine_memory_map(mem_map);

    struct system_memory_info mem_info;
    memset(&mem_info, 0, sizeof(struct system_memory_info));

    if (get_system_memory_info(mem_map, &mem_info) != 0) {
        printf("Unable to obtain system memory information");
        return -1;
    }

    const uint64_t largest_pfn = phys_to_pfn(mem_info.highest_mapped_mem_addr);
    /* Need to account for pfn 0 */
    const uint64_t nr_of_pfns = largest_pfn + 1;
    /* Nr of bytes needed so that we can fit 1 bit per pfn */
    const uint64_t bitmap_size = (nr_of_pfns >> 3) + (nr_of_pfns % 8 != 0);
    const uint64_t allocator_region_size = sizeof(struct pf_bitmap_allocator) + bitmap_size;

    virt_addr_t __directly_mapped pf_allocator_region;
    int ret = find_contiguous_memory_region(
            mem_map,
            allocator_region_size,
            &pf_allocator_region
    );

    if (ret != 0) {
        printf("No large-enough contiguous region of memory available for storing allocator metadata");
        return -1;
    }

    init_pf_allocator_metadata(
            pf_allocator_region,
            allocator_region_size,
            bitmap_size,
            &mem_info
    );

    if (early_initialize_pf_bitmap(mem_map, largest_pfn) != 0) {
        printf("Failed to initialize the page frame allocator bitmap");
        return -1;
    }

    return 0;
}

static int
__get_pages(const uint64_t nr, phys_addr_t *start_addr)
{
    const uint64_t free_frames =
        pf_allocator->total_usable_page_frames - pf_allocator->allocated_page_frames;

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

    toggle_bitmap_region(pfn_to_phys(pfn), nr);

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

    pf_allocator->allocated_page_frames += nr;

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

inline int
get_pages(const uint64_t nr, virt_addr_t __directly_mapped *start_addr)
{
    phys_addr_t phys_addr;

    if (__get_pages(nr, &phys_addr) != 0) {
        return -1;
    }

    *start_addr = phys_to_directly_mapped(phys_addr);
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
    toggle_bitmap_region(addr, nr);
    return 0;
}

inline int
free_page(const virt_addr_t __directly_mapped addr)
{
    return free_page_raw(directly_mapped_to_phys(addr));
}

inline int
free_pages(const uint64_t nr, const virt_addr_t __directly_mapped addr)
{
    return free_pages_raw(nr, directly_mapped_to_phys(addr));
}

