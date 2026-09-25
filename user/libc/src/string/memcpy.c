#include <string.h>
#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    if (!dest || !src || n == 0) return dest;

    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}
