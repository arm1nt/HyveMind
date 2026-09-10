#ifndef _HYVEMIND_STRING_H
#define _HYVEMIND_STRING_H

#include "hyvstdlib.h"

void *memset(void *dst, int c, size_t len);
void *memcpy(void *restrict dst, const void *restrict src, size_t len);
void *memmove(void *dst, const void *src, size_t len);
int memcmp(const void *s1, const void *s2, size_t len);

/* Both must be null terminated */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, const size_t len);
/* Must be null terminated. */
size_t strlen(const char *s);
/* The resulting string copy is null terminated */
char *strdup_nt(const char *s);
char *strndup_nt(const char *s, const size_t len);

uint64_t strtoul(char *str, char **endptr, bool *overflow);

#endif /* _HYVEMIND_STRING_H */

