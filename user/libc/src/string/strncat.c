#include <stdint.h>
#include <stddef.h>

/**
 * Anexa até 'n' caracteres da string 'src' ao fim da string 'dest'.
 * 
 * @param dest   Ponteiro para a string de destino (deve ter espaço suficiente).
 * @param src    Ponteiro para a string de origem.
 * @param n      Número máximo de caracteres a copiar de 'src'.
 * @return       Ponteiro para a string de destino inicial.
 */
char* strncat(char* dest, const char* src, size_t n) {
    if (!dest || !src) return dest;

    char* ptr = dest;

    while (*ptr != '\0') {
        ptr++;
    }

    while (n > 0 && *src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
        n--;
    }

    *ptr = '\0';

    return dest;
}
