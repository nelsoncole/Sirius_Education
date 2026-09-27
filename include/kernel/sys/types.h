/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: types.h
 *    Description: Definições de tipos de dados primitivos do sistema.
 *                 Garante a padronização e o tamanho fixo de identificadores
 *                 de processos, utilizadores e segurança na arquitetura x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <kernel/lib/stdint.h>

/* Identificadores de Segurança e Privilégios (Mapeados no sys_ident.c) */
typedef uint32_t           uid_t;      /* User ID (0 = root/supervisor, >0 = utilizadores) */
typedef uint32_t           gid_t;      /* Group ID (Identificador de grupos de segurança) */

#endif /* _TYPES_H_ */
