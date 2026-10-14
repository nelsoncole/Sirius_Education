/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ramfs.h
 *    Description: Subsistema do RamFS (Ram File System) e definições do driver.
 *                 Interface modular que expõe o inicializador de nós persistentes
 *                 em RAM e garante a consistência com as regras de herança e 
 *                 page cache do Virtual File System (VFS).
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _RAMFS_H_
#define _RAMFS_H_

void ramfs_init(void);

#endif