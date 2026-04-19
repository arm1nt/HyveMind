#include <stdint.h>
#include <stddef.h>
#include "string.h"

void *
memset(void *dst, int c, size_t len)
{
    uint8_t *p = dst;

    for (size_t i = 0; i < len; i++) {
        p[i] = (uint8_t)c;
    }

    return dst;
}

void *
memcpy(void *restrict dst, const void *restrict src, size_t len)
{
    uint8_t *restrict pdest = dst;
    const uint8_t *restrict psrc = src;

    for (size_t i = 0; i < len; i++) {
        pdest[i] = psrc[i];
    }

    return dst;
}

void *
memmove(void *dst, const void *src, size_t len)
{
    uint8_t *pdest = dst;
    const uint8_t *psrc = src;

    if ((uintptr_t)src > (uintptr_t)dst) {
        for (size_t i = 0; i < len; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dst) {
        for (size_t i = len; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dst;
}

int
memcmp(const void *s1, const void *s2, size_t len)
{
    const uint8_t *p1 = s1;
    const uint8_t *p2 = s2;

    for (size_t i = 0; i < len; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

