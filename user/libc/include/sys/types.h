/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: types.h
 *    Description: Definições de tipos de dados primitivos padrão exigidos pela
 *                 especificação POSIX e subsistemas do sistema operativo.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

/* 
 * ============================================================================
 * ABSTRAÇÕES ESPECÍFICAS DO SUBSISTEMA POSIX (Processos e Arquivos)
 * ============================================================================
 */

typedef int                pid_t;    /* Identificador de Processo (Process ID) */
typedef int                id_t;     /* Identificador genérico de ID */

typedef unsigned int       uid_t;    /* Identificador de Utilizador (User ID) */
typedef unsigned int       gid_t;    /* Identificador de Grupo (Group ID) */

typedef long               off_t;    /* Deslocamento em ficheiros (File Offset) */
typedef unsigned long      ino_t;    /* Número de Inode do Sistema de Ficheiros */
typedef unsigned int       mode_t;   /* Flags de permissões e modos de ficheiros */
typedef unsigned long      dev_t;    /* Identificador de Dispositivo de Hardware */
typedef unsigned int       nlink_t;  /* Contador de Hard Links em arquivos */

/* Gestão de Tempo e Sincronismo */
typedef long               time_t;   /* Representação do tempo em segundos Unix */
typedef long               suseconds_t; /* Tempo fracionado em microssegundos */

#endif