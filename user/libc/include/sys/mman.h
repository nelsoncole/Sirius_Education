/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mman.h
 *    Description: 
 * 
 *        Author:  Nelson Cole
 *   Created Date: 18/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _MMAN_H
#define _MMAN_H

#define MAP_SHARED      0x01    // Patilha o mapeamento. Alterações são visíveis para outros e gravadas no ficheiro.
#define MAP_PRIVATE     0x02    // Mapeamento privado (Copy-on-Write). As alteracoes Nao sao gravadas no ficheiro.
#define MAP_ANONYMOUS   0x20    // Mapeamento anonimo. A memoria nao e ligada a nenhum ficheiro (ignora o argumento fd).
#define MAP_ANON        MAP_ANONYMOUS // Sinonimo para compatibilidade historica.
#define MAP_FIXED       0x10    // Forca o kernel a usar extamente o endereco virual ('addr') fornecido pelo utilizador.
#define MAP_FAILED      ((void *)-1UL)

#define PROT_NONE       0x0     // A pagina nao pode ser acessada (gera um page fault imediato se houver tentativa de acesso).
#define PROT_READ       0x1     // A pagina pode ser lida
#define PROT_WRITE      0x2     // A pagina pode ser escrita
#define PROT_EXEC       0x4     // A pagina pode conter codigo para ser executado pelo CPU

void *mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset);
void munmap(void *addr, size_t length);

#endif