#include "mm.h"
#include "limine/helpers.h"
#include "printf.h"
#include "string.h"

#include <stddef.h>

#undef PRINT_PREFIX_NAME
#define PRINT_PREFIX_NAME "pf-allocator"

struct pf_bitmap_allocator {
    uint8_t *bitmap;
    uint64_t bitmap_size;

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
mark_bitmap_entry(const phys_addr_t addr)
{
    const uint64_t pfn = phys_addr_to_pfn(addr);
    const uint64_t byte_index = pfn >> 3;
    const uint64_t bit_index = pfn % 8;

    uint8_t *byte = &pf_allocator->bitmap[byte_index];
    *byte = *byte | U8_LSHIFT(1, bit_index);

    pf_allocator->allocated_page_frames++;
}

static int
early_init_page_frame_bitmap(struct limine_memmap_response *mem_map, const uint64_t offset)
{
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
            mark_bitmap_entry(j);
        }
    }

    /* Mark the pageframes used by the allocator's metadata as used */
    const phys_addr_t physical_pf_allocator_base = ((uint64_t) pf_allocator) - offset;
    const uint64_t pf_allocator_region_limit = physical_pf_allocator_base
        + sizeof(struct pf_bitmap_allocator)
        + pf_allocator->bitmap_size;

    for (uint64_t i = physical_pf_allocator_base; i < pf_allocator_region_limit; i += PAGE_SIZE) {
        mark_bitmap_entry(i);
    }

    return 0;
}

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

