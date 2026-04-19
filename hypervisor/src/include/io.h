#ifndef _HYVEMIND_IO_H
#define _HYVEMIND_IO_H

#include <stdint.h>

#define BUILD_IO_OUT(size, type)                                    \
    static inline void                                              \
    io_out##size(uint16_t io_port, type val)                        \
    {                                                               \
        asm volatile ("out %0, %1" :: "a"(val), "d"(io_port));      \
    }

#define BUILD_IO_IN(size, type)                                     \
    static inline type                                              \
    io_in##size(uint16_t io_port)                                   \
    {                                                               \
        int ret;                                                    \
        asm volatile ("in %1, %0" : "=a"(ret) : "d"(io_port));      \
        return ret;                                                 \
    }

BUILD_IO_OUT(b, uint8_t);
BUILD_IO_OUT(w, uint16_t);
BUILD_IO_OUT(d, uint32_t);
#undef BUILD_IO_OUT

BUILD_IO_IN(b, uint8_t);
BUILD_IO_IN(w, uint16_t);
BUILD_IO_IN(d, uint32_t);
#undef BUILD_IO_IN

#endif /* _HYVEMIND_IO_H */

