#include <stdio.h>
#include <unistd.h>

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp) 
{
    if (fp == NULL || ptr == NULL || size == 0 || nmemb == 0) return 0;

    size_t total_bytes = size * nmemb;
    
    // Invoca a sua chamada de sistema write() do VFS
    ssize_t bytes_written = write(fp->fd, ptr, total_bytes);
    
    if (bytes_written < 0) return 0;
    return (size_t)bytes_written / size;
}