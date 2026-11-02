#include "procyon_file.h"
#include <stdio.h>
#include <stdlib.h>

unsigned char *procyon_upload_file(const char *path, size_t *file_size)
{
    if (path == NULL || file_size == NULL)
        return NULL;

    *file_size = 0;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    unsigned char *buffer = malloc((size_t)size);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)size, file);

    fclose(file);

    if (bytes_read != (size_t)size) {
        free(buffer);
        return NULL;
    }

    *file_size = bytes_read;

    return buffer;
}