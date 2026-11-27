#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

extern int _resolve_absolute_path(char *out_buf, const char *pathname);

char *realpath(const char *path, char *resolved_path)
{
    if (!path || !*path)
    {
        errno = EINVAL;
        return NULL;
    }

    char absolute_path[PATH_MAX];

    if (_resolve_absolute_path(absolute_path, path) < 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (!resolved_path)
    {
        resolved_path = malloc(strlen(absolute_path) + 1);

        if (!resolved_path)
            return NULL;
    }

    strcpy(resolved_path, absolute_path);

    return resolved_path;
}