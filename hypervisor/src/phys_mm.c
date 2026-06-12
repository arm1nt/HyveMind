#include "hyvstdlib.h"
#include "phys_mm.h"
#include "string.h"
#include "asm/paging.h"

static sys_mem_info_t sys_mem_info;

const sys_mem_info_t *
init_system_memory_info(const boot_mem_info_t *info)
{
    hyv_memmap_t *mem_map = (hyv_memmap_t *) __phys_to_virt(
            info->mem_map,
            early_direct_mapping_offset
    );

    uint64_t total_size = 0;
    uint64_t early_usable_size = 0;
    uint64_t total_usable_size = 0;

    phys_addr_t lowest_mapped_paddr = ~((phys_addr_t) 0);
    phys_addr_t highest_mapped_paddr = 0;

    memmap_entry_t entry;
    for (uint64_t i = 0; i < mem_map->nr_entries; i++) {
        entry = mem_map->entry[i];

        lowest_mapped_paddr = MIN(entry.start, lowest_mapped_paddr);
        highest_mapped_paddr = MAX(entry.end, highest_mapped_paddr);

        if (entry.type == MEMMAP_USABLE) {
            early_usable_size += entry.length;
            total_usable_size += entry.length;
        } else if (entry.type == MEMMAP_BOOTLOADER_RECLAIMABLE) {
            total_usable_size += entry.length;
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

const sys_mem_info_t *
get_system_memory_info(void)
{
    return &sys_mem_info;
}

static inline memmap_region_type_t
limine_memmap_type_to_hyv_type(const int limine_type)
{
    switch (limine_type) {
        case LIMINE_MEMMAP_USABLE:
            return MEMMAP_USABLE;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return MEMMAP_BOOTLOADER_RECLAIMABLE;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            return MEMMAP_HYPERVISOR;
        default:
            return MEMMAP_RESERVED;
    }
}

boot_mem_info_t
get_boot_mem_info(
        const limine_memmap_t *limine_mem_map,
        const limine_exec_addr_info_t *exec_info
)
{
    boot_mem_info_t *info = (boot_mem_info_t *) boot_mem_info_scratch;

    info->hypervisor_region.paddr_start = exec_info->physical_base;
    info->hypervisor_region.vaddr_start = __vaddr(&__hypervisor_base);
    info->hypervisor_region.length =
        __vaddr(&__hypervisor_data_end) - __vaddr(&__hypervisor_base);

    const virt_addr_t memmap_vaddr =
        __vaddr(boot_mem_info_scratch) + sizeof(boot_mem_info_t);

    info->mem_map = (phys_addr_t) memmap_vaddr - early_direct_mapping_offset;

    hyv_memmap_t *memmap = (hyv_memmap_t *) memmap_vaddr;
    memmap->nr_entries = limine_mem_map->entry_count;

    struct limine_memmap_entry *limine_entry;
    memmap_entry_t *hyv_entry;
    for (uint64_t i = 0; i < limine_mem_map->entry_count; i++) {
        limine_entry = limine_mem_map->entries[i];
        hyv_entry = &memmap->entry[i];

        hyv_entry->start = limine_entry->base;
        hyv_entry->end = limine_entry->base + limine_entry->length - 1;
        hyv_entry->length= limine_entry->length;
        hyv_entry->type = limine_memmap_type_to_hyv_type(limine_entry->type);
    }

    /**
     * Save to return the pointed-to-struct since its not allocated on the stack,
     * but in the dedicated scratch space in the hypervisor image.
     */
    return *info;
}

