#ifndef _HYVEMIND_HALLOC_H
#define _HYVEMIND_HALLOC_H

#include <stdint.h>
#include <stddef.h>

int init_ptfl_allocator(void);

void* hmalloc(const size_t req);
void* hmalloc_align(const size_t req, const uint8_t alignment);

void hfree(const void *ptr);

#endif /* _HYVEMIND_HALLOC_H */

