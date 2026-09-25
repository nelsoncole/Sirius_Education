/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fcntl.h
 *    Description: Cabeçalho padrão POSIX para operações de controlo e flags
 *                 de abertura de ficheiros (File Control Options).
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* Modos de Acesso Fundamentais (Mapeados nos bits inferiores de flags) */
#define O_RDONLY    0x0000    /* Abre exclusivamente para leitura */
#define O_WRONLY    0x0001    /* Abre exclusivamente para escrita */
#define O_RDWR      0x0002    /* Abre para leitura e escrita binária */

/* Flags de Criação e Estado de Ficheiros (POSIX Bitmasks) */
#define O_CREAT     0x0200    /* Força a criação do ficheiro se não existir */
#define O_APPEND    0x0008    /* Posiciona o ponteiro no fim antes de cada escrita */
#define O_TRUNC     0x0400    /* Trunca o tamanho do ficheiro para 0 se já existir */
#define O_NONBLOCK  0x4000    /* Ativa o modo de E/S não-bloqueante */

#ifdef __cplusplus
extern "C" {
#endif

/* Assinatura da chamada de sistema regulamentar POSIX */
int open(const char *pathname, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif /* _FCNTL_H */