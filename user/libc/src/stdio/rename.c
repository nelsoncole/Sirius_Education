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
#include <unistd.h>
#include <string.h>

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 4096
#endif

int rename(const char *old, const char *new)
{
    // 1. Validação defensiva de ponteiros nulos ou vazios
    if (old == NULL || old[0] == '\0' || new == NULL || new[0] == '\0') 
    {
        return -1;
    }

    char old_absolute[MAX_PATH_LEN];
    char new_absolute[MAX_PATH_LEN];
    char pwd[MAX_PATH_LEN];
    int pwd_carregado = 0;

    // 2. RESOLVER O CAMINHO ANTIGO (old)
    if (old[0] == '/')
    {
        strcpy(old_absolute, old);
    }
    else
    {
        if (getcwd(pwd, MAX_PATH_LEN) == NULL) return -1;
        pwd_carregado = 1; // Evita chamar o getcwd duas vezes se o 'new' também for relativo
        sprintf(old_absolute, "%s/%s", pwd, old);
    }

    // 3. RESOLVER O CAMINHO NOVO (new)
    if (new[0] == '/')
    {
        strcpy(new_absolute, new);
    }
    else
    {
        if (!pwd_carregado)
        {
            if (getcwd(pwd, MAX_PATH_LEN) == NULL) return -1;
        }
        sprintf(new_absolute, "%s/%s", pwd, new);
    }

    // 4. Passa os ponteiros das strings absolutas e limpas para o Kernel
    int ret = (int)syscall2(SYS_RENAME, (uint64_t)old_absolute, (uint64_t)new_absolute);
    
    if (ret < 0) {
        return -1;
    }

    return 0; // Sucesso
}