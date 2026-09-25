#include <string.h>
#include <stddef.h>

size_t strlen(const char *s) {
    if (!s) return 0; // evita crash se ponteiro NULL

    const char *p = s;
    while (*p) p++;    // percorre até encontrar '\0'
    
    return (size_t)(p - s);
}