/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stat.h
 *    Description: Estruturas de metadados e macros de estado de ficheiros (stat)
 *                 para o espaço de utilizador do Sirius_Education OS.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 01/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _STAT_H_
#define _STAT_H_

#include "types.h"

/* 
 * Definições dos bits de modo de ficheiro (Permissions & File Types)
 * Ajustado para o VFS e sistemas FAT/NTFS do Sirius_Education
 */
#define S_IFMT   0170000   /* Máscara para o tipo de ficheiro */
#define S_IFREG  0100000   /* Ficheiro Regular */
#define S_IFDIR  0040000   /* Diretório */
#define S_IFCHR  0020000   /* Dispositivo de Caracteres (TTY/UART) */
#define S_IFBLK  0060000   /* Dispositivo de Blocos (Storage) */

/* Permissões básicas POSIX */
#define S_IRWXU  00700     /* Dono: Ler, Escrever e Executar */
#define S_IRUSR  00400     /* Dono: Ler */
#define S_IWUSR  00200     /* Dono: Escrever */
#define S_IXUSR  00100     /* Dono: Executar */

#define S_IRWXG  00070     /* Grupo: Ler, Escrever e Executar */
#define S_IRWXO  00007     /* Outros: Ler, Escrever e Executar */

/* Macros de verificação de tipo para o teu Interpretador de Comandos (sh.c) */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)

/*
 * Estrutura stat nativa de 64 bits do Sirius_Education
 * Alinhada com os tipos da tua árvore de diretórios do VFS
 */
struct stat {
    uint32_t st_dev;     /* Identificador do dispositivo de hardware */
    uint64_t st_ino;     /* Número do Inode ou entrada na FAT/NTFS */
    uint32_t st_mode;    /* Tipo do ficheiro e permissões de acesso */
    uint32_t st_nlink;   /* Número de hard links ativos */
    uint32_t st_uid;     /* ID do Utilizador dono do processo (proc->uid) */
    uint32_t st_gid;     /* ID do Grupo dono */
    uint32_t st_rdev;    /* ID do dispositivo caso seja um ficheiro especial (char/blk) */
    uint64_t st_size;    /* Tamanho total do ficheiro em bytes (file->size) */
    uint64_t st_blksize; /* Tamanho ideal de bloco de E/S para o driver disk */
    uint64_t st_blocks;  /* Número de blocos físicos alocados no disco */
    
    /* Timestamps de controlo temporal */
    uint64_t st_atime;   /* Data do último acesso */
    uint64_t st_mtime;   /* Data da última modificação */
    uint64_t st_ctime;   /* Data da última alteração de estado */
};

/* Protótipos das Syscalls expostas na tua libc/unistd do Ring 3 */
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);

#endif /* _STAT_H */