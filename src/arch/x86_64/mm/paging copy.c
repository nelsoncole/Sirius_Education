/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: paging.c
 *    Description: Implementação do sistema de paginação x86_64 do kernel.
 *
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 *  Modified Date: 29/08/2026
 *
 *        License: MIT
 * ============================================================================
 */


#include "paging.h"
#include <kernel/lib/string.h>
#include <kernel/kernel.h>
#include <stdint.h>


/*
 * ============================================================================
 * TABELAS GLOBAIS
 * ============================================================================
 */

PML4_TABLE *g_pml4 = 0;

PAGE_DIRECTORY_POINTER_TABLE *g_pdpt = 0;

PAGE_DIRECTORY *g_pd = 0;

PAGE_TABLE *g_pt = 0;


/*
 * Próxima entrada livre.
 */
uint64_t g_next_free_pt_index = 0;


/*
 * ============================================================================
 * ENABLE NX
 * ============================================================================
 *
 * Habilita EFER.NXE antes de utilizar o bit NX nas Page Tables.
 *
 * EFER = MSR 0xC0000080
 * NXE   = bit 11
 *
 * ============================================================================ */

static void enable_nxe(void)
{
    uint32_t eax;
    uint32_t edx;

    __asm__ __volatile__(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        : "=a"(eax), "=d"(edx)
        :
        : "ecx"
    );

    eax |= (1U << 11);

    __asm__ __volatile__(
        "mov $0xC0000080, %%ecx\n"
        "wrmsr\n"
        :
        : "a"(eax), "d"(edx)
        : "ecx", "memory"
    );
}


/*
 * ============================================================================
 * SETUP PAGING
 * ============================================================================
 */

void setup_paging(BOOT_INFO *boot_info)
{
    /*
     * ========================================================================
     * VARIÁVEIS
     * ========================================================================
     */

    uint64_t kernel_phys;
    uint64_t kernel_size;
    uint64_t kernel_pages;
    uint64_t kernel_pt_count;

    uint64_t framebuffer_phys;
    uint64_t framebuffer_size;
    uint64_t framebuffer_pages;

    uint64_t PML4_PHYSICAL;
    uint64_t PDPT_PHYSICAL;
    uint64_t PD_IDENTITY_PHYSICAL;
    uint64_t PD_KERNEL_PHYSICAL;
    uint64_t PT_PHYSICAL;


    PML4_TABLE *pml4;

    PAGE_DIRECTORY_POINTER_TABLE *pdpt;

    PAGE_DIRECTORY *pd_identity;
    PAGE_DIRECTORY *pd_kernel;

    PAGE_TABLE *pt;


    /*
     * ========================================================================
     * ENDEREÇOS FÍSICOS
     * ========================================================================
     */

    PML4_PHYSICAL =
        (uint64_t)boot_info->KernelAddress +
        PML4_PHYSICAL_OFFSET;

    PDPT_PHYSICAL =
        (uint64_t)boot_info->KernelAddress +
        PDPT_PHYSICAL_OFFSET;

    PD_IDENTITY_PHYSICAL =
        (uint64_t)boot_info->KernelAddress +
        PD_IDENTITY_PHYSICAL_OFFSET;

    PD_KERNEL_PHYSICAL =
        (uint64_t)boot_info->KernelAddress +
        PD_KERNEL_PHYSICAL_OFFSET;

    PT_PHYSICAL =
        (uint64_t)boot_info->KernelAddress +
        PT_PHYSICAL_OFFSET;


    /*
     * ========================================================================
     * ENDEREÇOS VIRTUAIS DAS TABELAS
     *
     * IMPORTANTE:
     *
     * Estas tabelas precisam estar acessíveis pelo endereço que está
     * atualmente ativo antes da troca do CR3.
     *
     * ========================================================================
     */

    pml4 =
        (PML4_TABLE *)(uintptr_t)PML4_ADDRESS;

    pdpt =
        (PAGE_DIRECTORY_POINTER_TABLE *)(uintptr_t)PDPT_ADDRESS;

    pd_identity =
        (PAGE_DIRECTORY *)(uintptr_t)PD_IDENTITY_ADDRESS;

    pd_kernel =
        (PAGE_DIRECTORY *)(uintptr_t)PD_KERNEL_ADDRESS;

    pt =
        (PAGE_TABLE *)(uintptr_t)PT_ADDRESS;


    g_pml4 = pml4;
    g_pdpt = pdpt;
    g_pd   = pd_kernel;
    g_pt   = pt;


    /*
     * ========================================================================
     * LIMPAR TABELAS
     * ========================================================================
     */

    memset(
        pml4,
        0,
        sizeof(PML4_TABLE));

    memset(
        pdpt,
        0,
        sizeof(PAGE_DIRECTORY_POINTER_TABLE));

    memset(
        pd_identity,
        0,
        sizeof(PAGE_DIRECTORY));

    memset(
        pd_kernel,
        0,
        sizeof(PAGE_DIRECTORY));

    memset(
        pt,
        0,
        sizeof(PAGE_TABLE) * NUM_PT_TABLES);


    /*
     * ========================================================================
     * DADOS DO KERNEL
     * ========================================================================
     */

    kernel_phys =
        (uint64_t)boot_info->KernelAddress;

    kernel_size =
        (uint64_t)boot_info->KernelMemorySize;


    /*
     * ========================================================================
     * PÁGINAS DO KERNEL
     * ========================================================================
     */

    kernel_pages =
        (kernel_size + PAGE_SIZE - 1) /
        PAGE_SIZE;


    /*
     * ========================================================================
     * PTs NECESSÁRIAS PARA O KERNEL
     *
     * PT[0] = Identity
     * PT[1...] = Kernel
     * ========================================================================
     */

    kernel_pt_count =
        (kernel_pages + 511) /
        512;


    /*
     * ========================================================================
     * PT INICIAL DO FRAMEBUFFER
     * ========================================================================
     */

    uint64_t framebuffer_pt_start =
        kernel_pt_count + 1;


    /*
     * PT[0] + Kernel + Framebuffer
     */
    if (framebuffer_pt_start >= NUM_PT_TABLES)
    {
        return;
    }


    /*
     * ========================================================================
     * IDENTITY MAPPING
     *
     * 0x0000000000000000
     * até
     * 0x00000000001FFFFF
     *
     * 2 MiB
     * ========================================================================
     */

    for (uint64_t i = 0; i < 512; i++)
    {
        pt[i].p = 1;
        pt[i].rw = 1;
        pt[i].us = 0;

        /*
         * Identity mapping pode executar durante o bootstrap.
         */
        pt[i].nx = 0;

        pt[i].frames = i;
    }


    /*
     * ========================================================================
     * PD IDENTITY[0] -> PT[0]
     * ========================================================================
     */

    pd_identity[0].p = 1;
    pd_identity[0].rw = 1;
    pd_identity[0].us = 0;
    pd_identity[0].ps = 0;

    pd_identity[0].phy_addr_pt =
        PT_PHYSICAL >> 12;


    /*
     * ========================================================================
     * PML4[0] -> PDPT
     * ========================================================================
     */

    pml4[0].p = 1;
    pml4[0].rw = 1;
    pml4[0].us = 0;

    pml4[0].phy_addr_pdpt =
        PDPT_PHYSICAL >> 12;


    /*
     * ========================================================================
     * PDPT[0] -> PD IDENTITY
     * ========================================================================
     */

    pdpt[0].p = 1;
    pdpt[0].rw = 1;
    pdpt[0].us = 0;

    pdpt[0].phy_addr_pd =
        PD_IDENTITY_PHYSICAL >> 12;


    /*
     * ========================================================================
     * HIGHER HALF
     *
     * 0xFFFFFFFF80000000
     *
     * PML4 = 511
     * PDPT = 510
     * PD   = 0
     * ========================================================================
     */

    pml4[511].p = 1;
    pml4[511].rw = 1;
    pml4[511].us = 0;

    pml4[511].phy_addr_pdpt =
        PDPT_PHYSICAL >> 12;


    /*
     * PDPT[510] -> PD KERNEL
     */

    pdpt[510].p = 1;
    pdpt[510].rw = 1;
    pdpt[510].us = 0;

    pdpt[510].phy_addr_pd =
        PD_KERNEL_PHYSICAL >> 12;


    /*
     * ========================================================================
     * MAPEAR KERNEL
     * ========================================================================
     */

    for (uint64_t page = 0;
         page < kernel_pages;
         page++)
    {
        /*
         * PT[0] é identity.
         *
         * Portanto:
         *
         * page 0 -> PT[1]
         */

        uint64_t pt_number =
            (page / 512) + 1;


        if (pt_number >= NUM_PT_TABLES)
        {
            return;
        }


        /*
         * Endereço físico.
         */

        uint64_t physical =
            kernel_phys +
            page * PAGE_SIZE;


        /*
         * Endereço virtual.
         */

        uint64_t virtual_addr =
            KERNEL_VIRTUAL_BASE +
            page * PAGE_SIZE;


        /*
         * Índice da PD.
         */

        uint64_t pd_index =
            (virtual_addr >> 21) & 0x1FF;


        /*
         * Índice da PT.
         */

        uint64_t pt_index =
            (virtual_addr >> 12) & 0x1FF;


        /*
         * Endereço físico da PT.
         */

        uint64_t current_pt_physical =
            PT_PHYSICAL +
            pt_number * sizeof(PAGE_TABLE);


        /*
         * PD KERNEL -> PT
         */

        pd_kernel[pd_index].p = 1;
        pd_kernel[pd_index].rw = 1;
        pd_kernel[pd_index].us = 0;
        pd_kernel[pd_index].ps = 0;

        pd_kernel[pd_index].phy_addr_pt =
            current_pt_physical >> 12;


        /*
         * Entrada global.
         */

        uint64_t pt_entry =
            (pt_number * 512) +
            pt_index;


        /*
         * PT -> KERNEL
         */

        pt[pt_entry].p = 1;
        pt[pt_entry].rw = 1;
        pt[pt_entry].us = 0;

        /*
         * Kernel executável.
         */

        pt[pt_entry].nx = 0;

        pt[pt_entry].frames =
            physical >> 12;
    }


    /*
     * ========================================================================
     * FRAMEBUFFER
     * ========================================================================
     */

    framebuffer_phys =
        (uint64_t)boot_info->Graphics.FrameBufferBase;

    framebuffer_size =
        (uint64_t)boot_info->Graphics.FrameBufferSize;


    /*
     * Limitar framebuffer para 64 MiB.
     */

    if (framebuffer_size > 0x04000000ULL)
    {
        framebuffer_size =
            0x04000000ULL;
    }


    /*
     * Número de páginas.
     */

    framebuffer_pages =
        (framebuffer_size + PAGE_SIZE - 1) /
        PAGE_SIZE;


    /*
     * ========================================================================
     * FRAMEBUFFER VIRTUAL
     *
     * 0xFFFF8000E0000000
     *
     * PML4 = 256
     * PDPT = 3
     * PD   = 256
     * ========================================================================
     */

    uint64_t framebuffer_virtual =
        KERNEL_VIDEO_VIRTUAL_BASE;


    uint64_t framebuffer_pml4_index =
        (framebuffer_virtual >> 39) & 0x1FF;


    uint64_t framebuffer_pdpt_index =
        (framebuffer_virtual >> 30) & 0x1FF;


    /*
     * ========================================================================
     * PML4[256] -> PDPT
     * ========================================================================
     */

    pml4[framebuffer_pml4_index].p = 1;
    pml4[framebuffer_pml4_index].rw = 1;
    pml4[framebuffer_pml4_index].us = 0;

    pml4[framebuffer_pml4_index].phy_addr_pdpt =
        PDPT_PHYSICAL >> 12;


    /*
     * ========================================================================
     * PDPT[3] -> PD KERNEL
     * ========================================================================
     */

    pdpt[framebuffer_pdpt_index].p = 1;
    pdpt[framebuffer_pdpt_index].rw = 1;
    pdpt[framebuffer_pdpt_index].us = 0;

    pdpt[framebuffer_pdpt_index].phy_addr_pd =
        PD_KERNEL_PHYSICAL >> 12;


    /*
     * ========================================================================
     * MAPEAR FRAMEBUFFER
     * ========================================================================
     */

    for (uint64_t page = 0;
         page < framebuffer_pages;
         page++)
    {
        /*
         * PT do framebuffer.
         */

        uint64_t pt_number =
            framebuffer_pt_start +
            (page / 512);


        if (pt_number >= NUM_PT_TABLES)
        {
            return;
        }


        /*
         * Endereço físico.
         */

        uint64_t physical =
            framebuffer_phys +
            page * PAGE_SIZE;


        /*
         * Endereço virtual.
         */

        uint64_t virtual_addr =
            KERNEL_VIDEO_VIRTUAL_BASE +
            page * PAGE_SIZE;


        /*
         * Índice da PD.
         */

        uint64_t pd_index =
            (virtual_addr >> 21) & 0x1FF;


        /*
         * Índice da PT.
         */

        uint64_t pt_index =
            (virtual_addr >> 12) & 0x1FF;


        /*
         * Endereço físico da PT.
         */

        uint64_t current_pt_physical =
            PT_PHYSICAL +
            pt_number * sizeof(PAGE_TABLE);


        /*
         * PD KERNEL -> PT FRAMEBUFFER
         */

        pd_kernel[pd_index].p = 1;
        pd_kernel[pd_index].rw = 1;
        pd_kernel[pd_index].us = 0;
        pd_kernel[pd_index].ps = 0;

        pd_kernel[pd_index].phy_addr_pt =
            current_pt_physical >> 12;


        /*
         * Entrada global.
         */

        uint64_t pt_entry =
            (pt_number * 512) +
            pt_index;


        /*
         * PT -> FRAMEBUFFER
         */

        pt[pt_entry].p = 1;
        pt[pt_entry].rw = 1;
        pt[pt_entry].us = 0;

        /*
         * Framebuffer NÃO executável.
         */
        pt[pt_entry].nx = 1;

        /*
         * Endereço físico.
         */
        pt[pt_entry].frames =
            physical >> 12;


        /*
         * Próxima entrada livre.
         */
        g_next_free_pt_index =
            pt_entry + 1;
    }


    /*
     * ========================================================================
     * HABILITAR NX
     *
     * Só fazemos isso depois de construir as tabelas.
     * ========================================================================
     */

    enable_nxe();


    /*
     * ========================================================================
     * DESABILITAR INTERRUPÇÕES
     * ========================================================================
     */

    __asm__ __volatile__(
        "cli"
        :
        :
        : "memory"
    );


    /*
     * ========================================================================
     * CARREGAR PML4 NO CR3
     *
     * CR3 recebe ENDEREÇO FÍSICO.
     * ========================================================================
     */

    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(PML4_PHYSICAL)
        : "memory"
    );


    /*
     * ========================================================================
     * PAGING ATIVO
     * ========================================================================
     */
}