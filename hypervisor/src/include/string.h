#ifndef _HYVEMIND_STRING_H
#define _HYVEMIND_STRING_H

#include <stddef.h>

void *memset(void *dst, int c, size_t len);
void *memcpy(void *restrict dst, const void *restrict src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
int memcmp(const void *s1, const void *s2, size_t len);
/* Both must be null terminated */
int strcmp(const char *s1, const char *s2);

#endif /* _HYVEMIND_STRING_H */

