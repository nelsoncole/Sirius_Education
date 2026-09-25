#include <string.h>
#include <stddef.h>


// compara os primeiros valores de bytes no limite de n
// retorna < 0 se s1 é menor do que s2
// retorna > 0 se s1 é maior do que s2
// retorna = 0 se s1 é igual a s2

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }

    return 0;
}