#include "halloc.h"
#include "hyvstdlib.h"
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

int
strcmp(const char *s1, const char *s2)
{
    char c1, c2;
    int counter = 0;

    while (1) {
        c1 = s1[counter];
        c2 = s2[counter];

        if (c1 != c2) {
            return 1;
        }

        if (c1 == '\0') {
            return 0;
        }

        counter++;
    }
}

int
strncmp(const char *s1, const char *s2, const size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (s1[i] != s2[i]) {
            return 1;
        }

        if (s1[i] == '\0') {
            return 0;
        }
    }

    return 0;
}

size_t
strlen(const char *s)
{
    size_t counter = 0;
    while (s[counter] != '\0') {
        counter++;
    }

    return counter;
}

char *
strdup_nt(const char *s)
{
    const size_t len = strlen(s) + 1;

    char *copy = (char *) hmalloc(len * sizeof(char));
    if (!copy) {
        return NULL;
    }

    memcpy(copy, s, len);
    return copy;
}

char *
strndup_nt(const char *s, const size_t len)
{
    const size_t src_len = strlen(s);
    const size_t dest_len = MIN(src_len, len);

    char *copy = (char *) hmalloc((dest_len+1) * sizeof(char));
    if (!copy) {
        return NULL;
    }

    memcpy(copy, s, dest_len);
    copy[dest_len] = '\0';
    return copy;
}

