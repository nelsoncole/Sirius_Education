#include <string.h>
#include <stddef.h>

void *memmove(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;

    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d > s && d < s + n) {
        // Copia de trás para frente para lidar com sobreposição
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    } else {
        // Copia normal
        while (n--) {
            *d++ = *s++;
        }
    }

    return dest;
}

