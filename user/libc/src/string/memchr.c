#include <string.h>
#include <stddef.h>

void *memchr(const void *buf, int c, size_t n) {
    if (!buf || n == 0) return NULL;

    const unsigned char *p = (const unsigned char *)buf;
    const unsigned char *end = p + n;
    unsigned char uc = (unsigned char)c;

    while (p != end) {
        if (*p == uc) {
            return (void*)p;
        }
        ++p;
    }

    return NULL;
}