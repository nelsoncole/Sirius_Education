#include <string.h>
#include <ctype.h>


// strcasecmp -- Comparação de duas strings, sem diferenciar maiúsculas e minúsculas
// retorna < 0 se str1 é menor do que str2
// retorna > 0 se str1 é maior do que str2
// retorna = 0 se str1 é igual a str2
int strcasecmp(const char *str1, const char *str2) {
    while (*str1 && *str2) {
        int d = tolower((unsigned char)*str1) - tolower((unsigned char)*str2);
        if (d != 0)
            return d;
        str1++;
        str2++;
    }
    return tolower((unsigned char)*str1) - tolower((unsigned char)*str2);
}