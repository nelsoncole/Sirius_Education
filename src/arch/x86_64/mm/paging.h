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
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _PAGING_H_
#define _PAGING_H_

#include <kernel/boot_info.h>

#define PAGE_SIZE 0x1000UL
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000UL
#define KERNEL_VIDEO_VIRTUAL_BASE 0xFFFF8000E0000000UL

/*
 * ============================================================
 * ENDEREÇO VIRTUAL DO BITMAP DE MEMÓRIA FÍSICA (PMM)
 * ============================================================
 *
 * Mapeado em 0xFFFF800000000000UL.
 * Utiliza o índice 256 da PML4 (partilhando o mesmo PDPT do Vídeo),
 * mas assume o Índice 0 da PDPT (o vídeo usa o índice 3).
 *
 * Espaço útil isolado de 1 GB, ideal para acomodar os 16 MB
 * necessários para gerir até 512 GB de memória RAM de forma limpa.
 * ============================================================
 */
#define KERNEL_BITMAP_VIRTUAL_BASE  0xFFFF800000000000UL



/*
 * ============================================================
 * ENDEREÇOS VIRTUAIS DAS PAGE TABLES
 * ============================================================
 */

#define PML4_ADDRESS 		0xFFFFFFFF80100000UL
#define PDPT_ADDRESS 		0xFFFFFFFF80101000UL
#define PD_IDENTITY_ADDRESS	0xFFFFFFFF80102000UL
#define PD_KERNEL_ADDRESS   0xFFFFFFFF80103000UL
#define PD_VIDEO_ADDRESS   	0xFFFFFFFF80104000UL
#define PD_BITMAP_ADDRESS   0xFFFFFFFF80105000UL
#define PT_ADDRESS   		0xFFFFFFFF80106000UL


/*
 * ============================================================
 * ENDEREÇOS FÍSICOS DAS PAGE TABLES
 * ============================================================
 *
 * As tabelas foram reservadas pelo bootloader.
 *
 * KernelAddress
 *      + 0x100000 -> PML4
 *      + 0x101000 -> PDPT
 *      + 0x102000 -> PD
 *      + 0x103000 -> PT
 *
 * ============================================================
 */

#define PML4_PHYSICAL_OFFSET		0x00100000UL
#define PDPT_PHYSICAL_OFFSET		0x00101000UL
#define PD_IDENTITY_PHYSICAL_OFFSET	0x00102000UL
#define PD_KERNEL_PHYSICAL_OFFSET	0x00103000UL
#define PD_VIDEO_PHYSICAL_OFFSET	0x00104000UL
#define PD_BITMAP_PHYSICAL_OFFSET	0x00105000UL
#define PT_PHYSICAL_OFFSET   		0x00106000UL


/*
 * ============================================================
 * ÁREA RESERVADA PARA AS PAGE TABLES
 * ============================================================
 *
 * PT_ADDRESS:
 *
 * 0xFFFFFFFF80106000
 *
 * até:
 *
 * 0xFFFFFFFF80200000
 *
 * 252 PTs disponíveis.
 *
 * ============================================================
 */

#define NUM_PT_TABLES 250

//
// ============================================================
// x86_64 PAGE TABLE ENTRY (PTE)
// 4 KB Page
// ============================================================
//

typedef struct _PAGE_TABLE
{
    unsigned long long  p 		:1;
	unsigned long long rw 		:1;
	unsigned long long us 		:1;
	unsigned long long pwt 		:1;
	unsigned long long pcd 		:1;
	unsigned long long a 		:1;
	unsigned long long d 		:1;
	unsigned long long pat 		:1;
	unsigned long long g 		:1;
	unsigned long long ign 		:3;
	unsigned long long frames 	:40;
	unsigned long long rs2 		:11;
	unsigned long long nx     	:1;  // No-Execute / Execute-Disable (Bit 63)

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


/*
 * ============================================================================
 * Estruturas Globais de Paginação (Tabelas de Páginas x86_64)
 * ============================================================================
 * 
 * Estas variáveis apontam para os blocos de memória virtual que gerem a MMU.
 * Foram mapeadas e instanciadas inicialmente no subsistema setup_paging.
 */

// Ponteiro para a tabela de Nível 4 (Page Map Level 4) - Raíz da paginação
extern PML4_TABLE *g_pml4;

// Ponteiro para a tabela de Nível 3 (Page Directory Pointer Table)
extern PAGE_DIRECTORY_POINTER_TABLE *g_pdpt;

// Ponteiro para a tabela de Nível 2 (Page Directory)
extern PAGE_DIRECTORY *g_pd;

// Ponteiro para a base do array de tabelas de Nível 1 (Page Tables)
extern PAGE_TABLE *g_pt;

// Inteiro para o próximo índice de entrada livre no array global de Page Tables (g_pt).
// Aponta para o início de cada bloco de 4 KB
extern unsigned long g_next_pt_number;

#endif