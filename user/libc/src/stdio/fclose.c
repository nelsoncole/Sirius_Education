#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int fclose(FILE *fp) 
{
    if (fp == NULL) return -1;

    // Invoca a sua chamada de sistema close() declarada em vfs_syscalls.c
    int ret = close(fp->fd); 

    // Liberta a memória da estrutura alocada no fdopen
    free(fp); 
    return ret;
}
