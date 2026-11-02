#ifndef PROCYON_FILE_H
#define PROCYON_FILE_H

#include <stddef.h>

unsigned char *procyon_upload_file(const char *path, size_t *file_size);

#endif