#ifndef CPU_MEMORY_H
#define CPU_MEMORY_H

#include <stdint.h>

/* ============================================================================
 * CPU FEATURES
 * ============================================================================
 */

typedef struct
{
    int sse2;
    int avx;
    int avx2;
    int osxsave;
} cpu_features_t;

void memcpy_init(void);

void *optimized_memcpy(void *dst, const void *src, size_t bytes);

#endif