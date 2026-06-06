/**
 * Power-of-two freelist allocator built on top of the page-frame allocator.
 */
#include "halloc.h"
#include "halloc_internal.h"
#include "hyvstdlib.h"
#include "pf_alloc.h"
#include "phys_mm.h"
#include "printf.h"
#include "string.h"
#include "spinlock.h"
#include "asm/paging.h"

#ifndef HALLOC_DEFAULT_ALIGNMENT
    #define HALLOC_DEFAULT_ALIGNMENT 8
#endif

/**
 * We could support larger allocation sizes, but that would come at the expense
 * of storing more data in the page struct. Since for large allocations we have
 * the page frame allocator already, this is a sensible limit.
 */
#define MAX_SUPPORTED_ALLOCATION_SIZE (1 << (12 + 8))

DEFINE_SPINLOCK(global_halloc_lock);

struct alloc_page {
    halloc_flags_t halloc_flags;
    uint8_t contig_allocated;
};

typedef struct free_chunk_hdr free_chunk_hdr_t;
struct free_chunk_hdr {
    free_chunk_hdr_t *next;
};

struct ptfl_allocator {
    uintptr_t bins[NR_OF_BINS];
    struct alloc_page *alloc_pages_array;
};

static struct ptfl_allocator ptfl_allocator;

static inline int
get_alloc_page_bin_index(const struct alloc_page *page)
{
    return page->halloc_flags & HALLOC_FLAGS_BIN_INDEX_MASK;
}

static inline void
set_page_range_halloc_flags(const virt_addr_t start, int nr_pages, const halloc_flags_t flags)
{
    for (int i = 0; i < nr_pages; i++) {
        const pfn_t pfn = phys_to_pfn(virt_to_phys(start + (i * PAGE_SIZE)));
        struct alloc_page *page = &ptfl_allocator.alloc_pages_array[pfn];
        page->halloc_flags = flags;
    }
}

static int
populate_bin(const int bin_index, const int chunk_shift, const int nr_pages)
{
    virt_addr_t pages_start;
    if (get_pages(nr_pages, &pages_start) != 0) {
        return -1;
    }

    set_page_range_halloc_flags(pages_start, nr_pages, HALLOC_FLAGS_MANAGED | bin_index);

    free_chunk_hdr_t next_chunk_hdr = { .next = NULL };

    const int chunk_size = 1 << chunk_shift;
    virt_addr_t chunk_pos = pages_start + (nr_pages << PAGE_SHIFT) - chunk_size;

    while (chunk_pos >= pages_start) {
        memcpy((void *) chunk_pos, &next_chunk_hdr, sizeof(free_chunk_hdr_t));
        next_chunk_hdr.next = (free_chunk_hdr_t *)chunk_pos;
        chunk_pos -= chunk_size;
    }

    ptfl_allocator.bins[bin_index] = (uintptr_t) next_chunk_hdr.next;
    return 0;
}

static inline int
repopulate_bin(const int bin_index, const struct bin_config *config)
{
    return populate_bin(bin_index, config->chunk_shift, config->repopulate_nr_pages);
}

int
init_ptfl_allocator(void)
{
    memset(&ptfl_allocator, 0, sizeof(struct ptfl_allocator));

    const sys_mem_info_t *sys_mem_info = get_system_memory_info();
    const uint64_t needed_pages = ROUND_UP(
            sys_mem_info->nr_of_pfns * sizeof(struct alloc_page),
            PAGE_SIZE
    );

    if (get_pages(needed_pages, (virt_addr_t *)&ptfl_allocator.alloc_pages_array) != 0) {
        printf("Failed to allocate memory for the 'alloc_pages_array' array");
        return -1;
    }

    struct bin_config config;
    for (int i = 0; i < NR_OF_BINS; i++) {
        config = bin_configs[i];

        if (populate_bin(config.bin_index, config.chunk_shift, config.nr_of_init_pages) != 0) {
            printf("Failed to initialize bin w/ index '%lu'", U64(config.bin_index));
            goto error_out;
        }
    }

    return 0;

error_out:
    free_pages(needed_pages, (virt_addr_t) ptfl_allocator.alloc_pages_array);
    return -1;
}

static inline int
size_to_bin_index(const size_t size)
{
    if (size <= 32) {
        return BIN_32B_INDEX;
    } else if (size <= 64) {
        return BIN_64B_INDEX;
    } else if (size <= 128) {
        return BIN_128B_INDEX;
    } else if (size <= 256) {
        return BIN_256B_INDEX;
    } else if (size <= 512) {
        return BIN_512B_INDEX;
    } else if (size <= 1024) {
        return BIN_1024B_INDEX;
    } else if (size <= 2048) {
        return BIN_2048B_INDEX;
    } else {
        return INVALID_BIN_INDEX;
    }
}

static void *
pf_fallback_allocation(const size_t req)
{
    const int pages_needed = ROUND_UP(req, PAGE_SIZE);
    virt_addr_t pages_start;
    if (get_pages(pages_needed, &pages_start) != 0) {
        return NULL;
    }

    spinlock_acquire(&global_halloc_lock);

    const pfn_t pfn = phys_to_pfn(virt_to_phys(pages_start));
    struct alloc_page *page = &ptfl_allocator.alloc_pages_array[pfn];
    page->contig_allocated = pages_needed;
    page->halloc_flags = HALLOC_FLAGS_PAGE_GRANULARITY | HALLOC_FLAGS_MANAGED;

    spinlock_release(&global_halloc_lock);

    return (void *) pages_start;
}

void *
hmalloc(const size_t req)
{
    return hmalloc_align(req, HALLOC_DEFAULT_ALIGNMENT);
}

void *
hmalloc_align(const size_t req, const uint8_t alignment)
{
    if (!req || !IS_POWER_OF_TWO(alignment)) {
        return NULL;
    }

    const size_t actual_req = MAX(req, alignment);
    const int bin_index = size_to_bin_index(actual_req);

    if (bin_index == INVALID_BIN_INDEX) {
        /* Fall back to the page frame allocator for large requests */

        if (actual_req > MAX_SUPPORTED_ALLOCATION_SIZE) {
            return NULL;
        }

        return pf_fallback_allocation(actual_req);
    } else {
        spinlock_acquire(&global_halloc_lock);

        const struct bin_config *bin_config = &bin_configs[bin_index];

        free_chunk_hdr_t *header = (free_chunk_hdr_t *) ptfl_allocator.bins[bin_index];
        if (header == NULL) {
            if (repopulate_bin(bin_index, bin_config) != 0) {
                return NULL;
            }
            header = (free_chunk_hdr_t *) ptfl_allocator.bins[bin_index];
        }

        ptfl_allocator.bins[bin_index] = (uintptr_t) header->next;
        memset((void *) header, 0, 1 << (bin_config->chunk_shift));

        spinlock_release(&global_halloc_lock);
        return (void *) header;
    }

    return NULL;
}

void
hfree(const void *ptr)
{
    spinlock_acquire(&global_halloc_lock);
    const pfn_t pfn = phys_to_pfn(virt_to_phys((virt_addr_t) ptr));
    struct alloc_page *page = &ptfl_allocator.alloc_pages_array[pfn];

    if (!(page->halloc_flags & HALLOC_FLAGS_MANAGED)) {
        /* Invalid pointer pointing to a page that is not managed by halloc */
        goto lock_release_out;
    }

    if (page->halloc_flags & HALLOC_FLAGS_PAGE_GRANULARITY) {
        /* Range allocated by falling back to the page frame allocator */
        const virt_addr_t page_range_start = (virt_addr_t) ptr;
        const int nr_pages = page->contig_allocated;
        set_page_range_halloc_flags(page_range_start, nr_pages, HALLOC_FLAGS_CLEAR);
        free_pages(nr_pages, page_range_start);
        goto lock_release_out;
    }

    const int bin_index = get_alloc_page_bin_index(page);
    const free_chunk_hdr_t freelist_node = {
        .next = (void *) ptfl_allocator.bins[bin_index]
    };
    memcpy((void *)ptr, &freelist_node, sizeof(free_chunk_hdr_t));
    ptfl_allocator.bins[bin_index] = (uintptr_t) ptr;

lock_release_out:
    spinlock_release(&global_halloc_lock);

    return;
}

