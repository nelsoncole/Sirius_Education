/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: paging.h
 *    Description: Estruturas de tabelas de páginas (PML4, PDPT, PD, PT) 
 *                 e definições para a paginação x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 27/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __PAGING_H__
#define __PAGING_H__

#include <kernel/boot_info.h>

//
// ============================================================
// x86_64 PAGE TABLE ENTRY (PTE)
// 4 KB Page
// ============================================================
//

typedef struct _PAGE_TABLE
{
    unsigned long long  p :1;
	unsigned long long rw :1;
	unsigned long long us :1;
	unsigned long long pwt :1;
	unsigned long long pcd :1;
	unsigned long long a :1;
	unsigned long long d :1;
	unsigned long long pat :1;
	unsigned long long g :1;
	unsigned long long ign :3;
	unsigned long long frames :40;
	unsigned long long rs2 :12;

} __attribute__((packed)) PAGE_TABLE;



//
// ============================================================
// x86_64 PAGE DIRECTORY ENTRY (PDE)
// ============================================================
//
// PS = 0:
//      Aponta para PAGE TABLE
//
// PS = 1:
//      Página de 2 MB
//
//

typedef struct _PAGE_DIRECTORY
{
    unsigned long long p :1;
	unsigned long long rw :1;
	unsigned long long us :1;
	unsigned long long pwt :1;
	unsigned long long pcd :1;
	unsigned long long a :1;
	unsigned long long ign1 :1;
	unsigned long long ps :1;
	unsigned long long ign2 :4;
	unsigned long long phy_addr_pt :40;
	unsigned long long rs2 :12;

} __attribute__((packed)) PAGE_DIRECTORY;



//
// ============================================================
// x86_64 PAGE DIRECTORY POINTER TABLE ENTRY (PDPTE)
// ============================================================
//
// PS = 0:
//      Aponta para PAGE DIRECTORY
//
// PS = 1:
//      Página de 1 GB
//
//

typedef struct _PAGE_DIRECTORY_POINTER_TABLE
{
    unsigned long long p :1;
	unsigned long long rw :1;
	unsigned long long us :1;
	unsigned long long pwt :1;
	unsigned long long pcd :1;
	unsigned long long rs1 :4;
	unsigned long long ign :3;
	unsigned long long phy_addr_pd :40;
	unsigned long long rs2 :12;

} __attribute__((packed)) PAGE_DIRECTORY_POINTER_TABLE;



//
// ============================================================
// x86_64 PML4 ENTRY
// ============================================================
//
// Aponta para PAGE DIRECTORY POINTER TABLE.
//
// Bits 6, 7 e 8 são reservados.
// Não existe PS nem G no PML4E.
//
//

typedef struct _PML4_TABLE
{
    unsigned long long p :1;
	unsigned long long rw :1;
	unsigned long long us :1;
	unsigned long long pwt :1;
	unsigned long long pcd :1;
	unsigned long long rs1 :4;
	unsigned long long ign :3;
	unsigned long long phy_addr_pdpt :40;
	unsigned long long rs2 :12;

} __attribute__((packed)) PML4_TABLE;

#endif