/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: rename.c
 *    Description: Implementação da chamada de sistema rename exigida por POSIX.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/usyscall.h>

int rename(const char *old, const char *new)
{
    if (old == NULL || old[0] == '\0' || new == NULL || new[0] == '\0') 
    {
        return -1;
    }

    // Passa o ponteiro da string antiga e o da nova string para o Kernel
    return (int)syscall2(SYS_RENAME, (uint64_t)old, (uint64_t)new);
}