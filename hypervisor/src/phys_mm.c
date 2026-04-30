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

static struct pf_bitmap_allocator *pf_allocator = NULL;

/**
 * @total_size ... Total size of all memory regions combined
 * @early_usable_size ... memory regions marked as 'USABLE'
 * @total_usable_size ...  includes BOOTLOADER_RECLAIMABLE regions
 */
struct system_memory_info {
    uint64_t total_size;
    uint64_t early_usable_size;
    uint64_t total_usable_size;

    phys_addr_t lowest_mapped_addr;
    phys_addr_t highest_mapped_addr;
};

static void
print_limine_memory_map(struct limine_memmap_response *mem_map)
{
    printf("----------------------------------------------------");

    uint64_t counter = 0;
    struct limine_memmap_entry *entry;

    while (counter < mem_map->entry_count) {
        entry = mem_map->entries[counter];

        printf(
                "[%s]\t\t %lu - %lu \t (len: %lu)",
                limine_mem_type_to_string[entry->type],
                entry->base,
                entry->base + entry->length,
                entry->length
        );

        counter++;
    }

    printf("----------------------------------------------------");
}

static int
get_system_memory_info(
        struct limine_memmap_response *mem_map,
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
    mem_info->early_usable_size = early_usable_size;
    mem_info->total_usable_size = total_usable_size;

    mem_info->lowest_mapped_addr = lowest_mapped;
    mem_info->highest_mapped_addr = highest_mapped;

    return 0;
}

static int
find_physical_memory_region(
        struct limine_memmap_response *mem_map,
        uint64_t direct_mapping_offset,
        uint64_t req_mem_region_size,
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

        if (entry->length > req_mem_region_size) {
            /* Its guaranteed by the bootloader that this address is page-aligned */
            *region_start_addr = (virt_addr_t) (entry->base + direct_mapping_offset);
            return 0;
        }
    }

    return -1;
}

static inline void
init_pf_allocator_metadata(
        const virt_addr_t pf_alloc_metadata_region,
        const uint64_t pf_alloc_metadata_region_size,
        const uint64_t bitmap_size,
        struct system_memory_info *mem_info
)
{
    pf_allocator = (struct pf_bitmap_allocator *) pf_alloc_metadata_region;
    memset(pf_allocator, 0, pf_alloc_metadata_region_size);

    pf_allocator->total_usable_page_frames = mem_info->total_usable_size / PAGE_SIZE;
    pf_allocator->bitmap_size = bitmap_size;
    pf_allocator->bitmap = (uint8_t *) (pf_alloc_metadata_region + sizeof(struct pf_bitmap_allocator));
}

static inline void
toggle_bitmap_entry(const phys_addr_t addr)
{
    const uint64_t pfn = phys_addr_to_pfn(addr);
    const uint64_t byte_index = pfn >> 3;
    const uint64_t bit_index = pfn % 8;

    uint8_t *byte = &pf_allocator->bitmap[byte_index];
    *byte = *byte ^ U8_RSHIFT(128, bit_index);

    pf_allocator->allocated_page_frames++;
}

static int
early_init_page_frame_bitmap(struct limine_memmap_response *mem_map, const uint64_t offset)
{
    /**
     * TODO: If we don't have enough page frames to completely fill the last byte
     * in the bitmap, set those bits to 1.
     */

    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            continue;
        }

        /**
         * If the region base is not page-aligned and the length is not a
         * multiple of the page size, we mark more than the region as used.
         * This is fine, since usable and bootloader reclaimable regions
         * are page-aligned.
         */
        for (uint64_t j = entry->base; j < entry->base + entry->length; j += PAGE_SIZE) {
            toggle_bitmap_entry(j);
        }
    }

    /* Mark the pageframes used by the allocator's metadata as used */
    const phys_addr_t physical_pf_allocator_base = ((uint64_t) pf_allocator) - offset;
    const uint64_t pf_allocator_region_limit = physical_pf_allocator_base
        + sizeof(struct pf_bitmap_allocator)
        + pf_allocator->bitmap_size;

    for (uint64_t i = physical_pf_allocator_base; i < pf_allocator_region_limit; i += PAGE_SIZE) {
        toggle_bitmap_entry(i);
    }

    return 0;
}

int
get_pages(const uint64_t nr, phys_addr_t __directly_mapped *start_addr)
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
    *start_addr = (pfn_to_phys_addr(pfn) + DIRECT_MAPPING_OFFSET);

    /* Mark pf as used in the bitmap */
    for(uint64_t i = 0; i < nr; i ++) {
        toggle_bitmap_entry((*start_addr -DIRECT_MAPPING_OFFSET)+ i*PAGE_SIZE);
    }

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

int
early_init_page_frame_allocator(
        struct limine_memmap_response *mem_map,
        const uint64_t direct_mapping_offset
)
{
    print_limine_memory_map(mem_map);

    struct system_memory_info mem_info;
    if (get_system_memory_info(mem_map, &mem_info) != 0) {
        printf("Unable to obtain system memory information");
        return -1;
    }

    const uint64_t highest_pfn = phys_addr_to_pfn(mem_info.highest_mapped_addr);
    const uint64_t bitmap_size = (highest_pfn >> 3) + 1;
    const uint64_t pf_allocator_region_size = sizeof(struct pf_bitmap_allocator) + bitmap_size;

    __directly_mapped virt_addr_t pf_alloc_metadata_region;
    int ret = find_physical_memory_region(
            mem_map,
            direct_mapping_offset,
            pf_allocator_region_size,
            &pf_alloc_metadata_region
    );

    if (ret != 0) {
        printf(
            "Failed to find a suitable memory region to store the pf-allocator"
            "metadata in"
        );
        return -1;
    }

    init_pf_allocator_metadata(
            pf_alloc_metadata_region,
            pf_allocator_region_size,
            bitmap_size,
            &mem_info
    );

    if (early_init_page_frame_bitmap(mem_map, direct_mapping_offset) != 0) {
        printf("Failed to initialize page frame allocator bitmap");
        return -1;
    }

    return 0;
}

