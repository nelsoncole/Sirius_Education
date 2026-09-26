#include <stdio.h>
#include <time.h>
#include <string.h>

size_t strftime(char *restrict s, size_t maxsize,
                const char *restrict format,
                const struct tm *restrict timeptr)
{
    char buffer[256]; // buffer temporário para montagem
    size_t pos = 0;

    for (const char *p = format; *p && pos < maxsize - 1; p++) {
        if (*p != '%') {
            buffer[pos++] = *p;
            continue;
        }

        p++; // avança para o especificador
        char temp[32] = {0};

        switch (*p) {
            case 'Y': // ano completo
                snprintf(temp, sizeof(temp), "%04d", timeptr->tm_year + 1900);
                break;
            case 'm': // mês (01-12)
                snprintf(temp, sizeof(temp), "%02d", timeptr->tm_mon + 1);
                break;
            case 'd': // dia (01-31)
                snprintf(temp, sizeof(temp), "%02d", timeptr->tm_mday);
                break;
            case 'H': // hora (00-23)
                snprintf(temp, sizeof(temp), "%02d", timeptr->tm_hour);
                break;
            case 'M': // minuto (00-59)
                snprintf(temp, sizeof(temp), "%02d", timeptr->tm_min);
                break;
            case 'S': // segundo (00-59)
                snprintf(temp, sizeof(temp), "%02d", timeptr->tm_sec);
                break;
            case '%': // '%' literal
                snprintf(temp, sizeof(temp), "%%");
                break;
            default: // especificador não suportado, copia literal
                temp[0] = '%';
                temp[1] = *p;
                temp[2] = '\0';
                break;
        }

        size_t len = strlen(temp);
        if (pos + len >= maxsize - 1)
            break; // evitar overflow
        memcpy(&buffer[pos], temp, len);
        pos += len;
    }

    buffer[pos] = '\0';
    strncpy(s, buffer, maxsize);
    return pos;
}
