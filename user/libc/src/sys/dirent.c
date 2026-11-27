#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/usyscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define DIRENT_BUFFER_SIZE 4096

struct __dirstream
{
    int fd;
    unsigned char *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    size_t buffer_len;
    struct dirent *entry;
};

DIR *opendir(const char *name)
{
    if (!name)
        return NULL;

    int fd = open(name, O_RDONLY, 0);

    if (fd < 0)
        return NULL;

    DIR *dir = malloc(sizeof(DIR));

    if (!dir)
    {
        close(fd);
        return NULL;
    }

    memset(dir, 0, sizeof(DIR));

    dir->buffer = malloc(DIRENT_BUFFER_SIZE);

    if (!dir->buffer)
    {
        close(fd);
        free(dir);
        return NULL;
    }

    dir->fd = fd;
    dir->buffer_size = DIRENT_BUFFER_SIZE;
    dir->buffer_pos = 0;
    dir->buffer_len = 0;
    dir->entry = NULL;

    return dir;
}

struct dirent *readdir(DIR *dirp)
{
    if (!dirp)
        return NULL;

    for (;;)
    {
        if (dirp->buffer_pos >= dirp->buffer_len)
        {
            int nread = (int)syscall3(
                SYS_GETDENTS,
                (uint64_t)dirp->fd,
                (uint64_t)dirp->buffer,
                (uint64_t)dirp->buffer_size
            );

            if (nread <= 0)
            {
                dirp->buffer_pos = 0;
                dirp->buffer_len = 0;
                return NULL;
            }

            dirp->buffer_pos = 0;
            dirp->buffer_len = (size_t)nread;
        }

        if (dirp->buffer_len - dirp->buffer_pos < offsetof(struct dirent, d_name) + 1)
            return NULL;

        struct dirent *entry = (struct dirent *)(dirp->buffer + dirp->buffer_pos);

        size_t remaining = dirp->buffer_len - dirp->buffer_pos;

        if (entry->d_reclen < offsetof(struct dirent, d_name) + 1 ||
            entry->d_reclen > remaining)
        {
            dirp->buffer_pos = dirp->buffer_len;
            return NULL;
        }

        dirp->buffer_pos += entry->d_reclen;

        size_t name_size = entry->d_reclen - offsetof(struct dirent, d_name);

        if (!memchr(entry->d_name, '\0', name_size))
            continue;

        dirp->entry = entry;

        return dirp->entry;
    }
}

int closedir(DIR *dirp)
{
    if (!dirp)
        return -1;

    int result = close(dirp->fd);

    free(dirp->buffer);
    free(dirp);

    return result;
}


/*
#include <dirent.h>
#include <stdio.h>

void listar_diretorio(const char *path)
{
    DIR *dir = opendir(path);

    if (!dir)
        return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        printf("%s\n", entry->d_name);

        if (entry->d_type == DT_DIR)
            printf("Diretoria\n");
        else if (entry->d_type == DT_REG)
            printf("Ficheiro\n");
    }

    closedir(dir);
}*/