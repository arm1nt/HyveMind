#include "phys_mm.h"
#include "string.h"
#include "asm/paging.h"

static sys_mem_info_t sys_mem_info;

const sys_mem_info_t *
init_system_memory_info(const struct limine_memmap_response *mem_map)
{
    uint64_t total_size = 0;
    uint64_t early_usable_size = 0;
    uint64_t total_usable_size = 0;

    phys_addr_t lowest_mapped_paddr = ~((phys_addr_t) 0);
    phys_addr_t highest_mapped_paddr = 0;

    struct limine_memmap_entry *entry;
    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

        lowest_mapped_paddr = (entry->base < lowest_mapped_paddr)
            ? entry->base
            : lowest_mapped_paddr;

        highest_mapped_paddr = ((entry->base + entry->length -1) > highest_mapped_paddr)
            ? (entry->base + entry->length - 1)
            : highest_mapped_paddr;

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            early_usable_size += entry->length;
            total_usable_size += entry->length;
        } else if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            total_usable_size += entry->length;
        }
    }

    memset(&sys_mem_info, 0, sizeof(sys_mem_info_t));

    sys_mem_info.total_size = total_size;
    sys_mem_info.early_usable_memory = early_usable_size;
    sys_mem_info.total_usable_memory = total_usable_size;

    sys_mem_info.lowest_mapped_addr = lowest_mapped_paddr;
    sys_mem_info.highest_mapped_addr = highest_mapped_paddr;

    const uint64_t largest_pfn = phys_to_pfn(highest_mapped_paddr);
    sys_mem_info.largest_mapped_pfn = largest_pfn;
    /* Account for pfn '0' */
    sys_mem_info.nr_of_pfns = largest_pfn + 1;

    return &sys_mem_info;
}

inline const sys_mem_info_t *
get_system_memory_info(void)
{
    return &sys_mem_info;
}

