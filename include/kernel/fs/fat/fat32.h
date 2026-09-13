/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fat32.h
 *    Description: Interface do Driver do Sistema de Ficheiros FAT32.
 *                 Fornece os protótipos de inicialização e registo do driver
 *                 para o subsistema do Virtual File System (VFS).
 * 
 *         Author: Nelson Cole
 *   Created Date: 12/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 12/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _FAT32_H_
#define _FAT32_H_

/**
 * Inicializa o subsistema interno do FAT32 e efetua o registo do driver
 * no catálogo global de sistemas de ficheiros do VFS.
 */
void fat32_init(void);

#endif /* _FAT32_H_ */