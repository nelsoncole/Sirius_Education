#include <string.h>

char *strrchr(const char *s, int c)
{
    const char *p = s + strlen(s);

    while (p >= s)
    {
        if (*p == (char)c)
            return (char*)p;

        p--;
    }

    return NULL;
}