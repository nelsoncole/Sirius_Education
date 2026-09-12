/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: paging.c
 *    Description: Implementação do sistema de paginação x86_64 do kernel.
 *                 Cria e inicializa as tabelas PML4, PDPT, PD e PT,
 *                 estabelece o mapeamento de memória física para virtual,
 *                 incluindo o mapeamento do kernel no higher half e do
 *                 framebuffer de vídeo, e configura o registrador CR3 com o
 *                 endereço físico da tabela PML4.
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

#include <kernel/arch/x86_64/mm/paging.h>
#include <kernel/lib/string.h>
#include <kernel/lib/stdint.h>
#include <kernel/kernel.h>


/*
 * Rastreia o número da próxima Tabela de Páginas (PT) de 4 KB inteira livre 
 * no array global (g_pt). Aponta para o início de cada bloco de 4 KB, 
 * evitando sobreposições e garantindo alocações contíguas na Higher-Half.
 */
unsigned long g_next_pt_number = 0;

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


void
setup_paging(
    BOOT_INFO *boot_info
)
{
    unsigned long kernel_phys;
    unsigned long kernel_size;
    unsigned long kernel_pages;

    unsigned long framebuffer_phys;
    unsigned long framebuffer_size;
    unsigned long framebuffer_pages;

    unsigned long PML4_PHYSICAL;
    unsigned long PDPT_PHYSICAL;
    unsigned long PDPT_IDENTITY_PHYSICAL;
    unsigned long PDPT_256_PHYSICAL;
    unsigned long PD_PHYSICAL;
    unsigned long PD_IDENTITY_PHYSICAL;
    unsigned long PD_VIDEO_PHYSICAL;
    unsigned long PT_PHYSICAL;


    PML4_TABLE *pml4;
    PAGE_DIRECTORY_POINTER_TABLE *pdpt;
    PAGE_DIRECTORY_POINTER_TABLE *pdpt_identity;
    PAGE_DIRECTORY_POINTER_TABLE *pdpt_256;
    PAGE_DIRECTORY *pd;
    PAGE_DIRECTORY *pd_identity;
    PAGE_DIRECTORY *pd_video;
    PAGE_DIRECTORY *pd_bitmap;
    PAGE_TABLE *pt;


    /*
     * ========================================================
     * ENDEREÇOS FÍSICOS DAS TABELAS
     * ========================================================
     */

    PML4_PHYSICAL = boot_info->KernelAddress + PML4_PHYSICAL_OFFSET;
    PDPT_PHYSICAL = boot_info->KernelAddress + PDPT_PHYSICAL_OFFSET;
    PDPT_IDENTITY_PHYSICAL = boot_info->KernelAddress + PDPT_IDENTITY_PHYSICAL_OFFSET;
    PDPT_256_PHYSICAL = boot_info->KernelAddress + PDPT_256_PHYSICAL_OFFSET;
    PD_PHYSICAL = boot_info->KernelAddress + PD_KERNEL_PHYSICAL_OFFSET;
    PD_IDENTITY_PHYSICAL = boot_info->KernelAddress + PD_IDENTITY_PHYSICAL_OFFSET;  
    PD_VIDEO_PHYSICAL = boot_info->KernelAddress + PD_VIDEO_PHYSICAL_OFFSET;
    PT_PHYSICAL = boot_info->KernelAddress + PT_PHYSICAL_OFFSET;


    /*
     * ========================================================
     * ENDEREÇOS DAS TABELAS
     *
     * Neste ponto o kernel ainda está usando o espaço virtual
     * fornecido pelo ambiente anterior.
     *
     * ========================================================
     */

    pml4 = (PML4_TABLE *)PML4_ADDRESS;
    pdpt = (PAGE_DIRECTORY_POINTER_TABLE *)PDPT_ADDRESS; 
    pdpt_identity = (PAGE_DIRECTORY_POINTER_TABLE *)PDPT_IDENTITY_ADDRESS;
    pdpt_256 = (PAGE_DIRECTORY_POINTER_TABLE *)PDPT_256_ADDRESS;
    pd = (PAGE_DIRECTORY *)PD_KERNEL_ADDRESS;
    pd_identity = (PAGE_DIRECTORY *)PD_IDENTITY_ADDRESS;
    pd_video = (PAGE_DIRECTORY *)PD_VIDEO_ADDRESS;
    pd_bitmap = (PAGE_DIRECTORY *)PD_BITMAP_ADDRESS;
    pt = (PAGE_TABLE *)PT_ADDRESS;


    /*
     * ========================================================
     * LIMPAR PML4
     * ========================================================
     */

    memset(pml4,0,sizeof(PML4_TABLE) * 512);

    /*
     * ========================================================
     * LIMPAR PDPT
     * ========================================================
     */

    memset(pdpt,0,sizeof(PAGE_DIRECTORY_POINTER_TABLE) * 512);
    memset(pdpt_identity,0,sizeof(PAGE_DIRECTORY_POINTER_TABLE) * 512);
    memset(pdpt_256,0,sizeof(PAGE_DIRECTORY_POINTER_TABLE) * 512);


    /*
     * ========================================================
     * LIMPAR PD
     * ========================================================
     */

    memset(pd,0,sizeof(PAGE_DIRECTORY) * 512);
    memset(pd_identity,0,sizeof(PAGE_DIRECTORY) * 512);
    memset(pd_video,0,sizeof(PAGE_DIRECTORY) * 512);
    memset(pd_bitmap,0,sizeof(PAGE_DIRECTORY) * 512);


    /*
     * ========================================================
     * LIMPAR TODAS AS PAGE TABLES
     * ========================================================
     */

    memset(pt,0,sizeof(PAGE_TABLE) * 512 * NUM_PT_TABLES);


    /*
     * ========================================================
     * DADOS DO KERNEL
     * ========================================================
     */

    kernel_phys = boot_info->KernelAddress;
    kernel_size = boot_info->KernelMemorySize;

    /*
     * ========================================================
     * QUANTIDADE DE PÁGINAS DO KERNEL
     * ========================================================
     */

    kernel_pages = (kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
     * ========================================================
     * IDENTITY MAPPING & PML4[0] -> PDPT
     *
     * Intervalo: 0x0000000000000000 até 0x00000000001FFFFF (2 MiB)
     * 
     * O PT[0] é usado exclusivamente para o mapeamento de identidade.
     * Portanto:
     *   - PT[0]    = Identity Mapping (1:1)
     *   - PT[1...] = Kernel Mappings
     * ========================================================
     */
    /*
     * ============================================================================
     * IDENTITY MAPPING ISOLADO (EXATAMENTE 1 MiB COMPATÍVEL COM VMWARE/VBOX)
     *
     * Intervalo: 0x0000000000000000 até 0x00000000000FFFFF (1 MiB / 256 Páginas)
     * 
     * Mapeia de forma 1:1 o primeiro megabyte físico. É crucial para que os 
     * APs acessem o código do trampolim em 0x8000 durante a subida de modo.
     * ============================================================================
     */
    for (unsigned long i = 0; i < 256; i++)
    {
        unsigned long physical = 0 + (i * PAGE_SIZE);
        pt[i].p = 1;
        pt[i].rw = 1;
        pt[i].us = 0;
        pt[i].frames = physical >> 12;
        pt[i].nx = 0; // Deve permitir execução de código (Bootstrap de 16-bits)
    }

    /* 
     * O array global de Page Tables avança.
     * Como a PT[0] foi inteiramente reservada para a Identidade de 1 MB, 
     * o mapeamento do Higher-Half do Kernel começará a partir da PT[1].
     */
    g_next_pt_number = 1;

    /*
     * ============================================================================
     * CONEXÃO FÍSICA DOS RAMOS DA ARVORE VIRTUAL
     * ============================================================================
     */

    // RAMO 1: Liga o Diretório Baixo à Page Table de Identidade
    pd_identity[0].p = 1;
    pd_identity[0].rw = 1;
    pd_identity[0].us = 0;
    pd_identity[0].ps = 0;
    pd_identity[0].phy_addr_pt = PT_PHYSICAL >> 12;

    // RAMO 2: Liga a PDPT Baixa ao Diretório de Identidade Físico
    pdpt_identity[0].p = 1;
    pdpt_identity[0].rw = 1;
    pdpt_identity[0].us = 0;
    pdpt_identity[0].phy_addr_pd = PD_IDENTITY_PHYSICAL >> 12;

    // RAMO 3: Liga a Raiz PML4 à PDPT Baixa no índice 0
    pml4[0].p = 1;
    pml4[0].rw = 1;
    pml4[0].us = 0;
    pml4[0].phy_addr_pdpt = PDPT_IDENTITY_PHYSICAL >> 12;

    /*
     * ========================================================
     * PML4 -> PDPT
     *
     * Higher Half:
     *
     * 0xFFFFFFFF80000000
     *
     * PML4[511]
     * ========================================================
     */

    pml4[511].p  = 1;
    pml4[511].rw = 1;
    pml4[511].us = 0;

    pml4[511].phy_addr_pdpt = PDPT_PHYSICAL >> 12;


    /*
     * ========================================================
     * PDPT -> PD
     *
     * Para:
     *
     * 0xFFFFFFFF80000000
     *
     * PDPT = 510
     * ========================================================
     */

    pdpt[510].p  = 1;
    pdpt[510].rw = 1;
    pdpt[510].us = 0;

    pdpt[510].phy_addr_pd = PD_PHYSICAL >> 12;


     /*
     * ========================================================
     * MAPEAR KERNEL
     *
     * Virtual:  0xFFFFFFFF80000000
     * Physical: boot_info->KernelAddress
     * ========================================================
     */

    for (unsigned long page = 0;  page < kernel_pages; page++)
    {
        /*
         * PT[0] está reservada para identity.
         *
         * Portanto o kernel começa em PT[1].
         */
        unsigned long pt_number = (page / 512) + g_next_pt_number;

        /*
         * Não ultrapassar as PTs disponíveis.
         */

        if (pt_number >= NUM_PT_TABLES)
        {
            break;
        }

        /*
         * Endereço físico da página.
         */

        unsigned long physical = kernel_phys + (page * PAGE_SIZE);
        /*
         * Endereço virtual do kernel.
         */

        unsigned long virtual_addr = KERNEL_VIRTUAL_BASE + (page * PAGE_SIZE);

        /*
         * Índices x86_64.
         */

        unsigned long pd_index = (virtual_addr >> 21) & 0x1FF;
        unsigned long pt_index = (virtual_addr >> 12) & 0x1FF;

        /*
         * Endereço físico da PT.
         */

        unsigned long current_pt_physical = PT_PHYSICAL + (pt_number * PAGE_SIZE);
        /*
         * ====================================================
         * PD -> PT
         * ====================================================
         */

        pd[pd_index].p  = 1;
        pd[pd_index].rw = 1;
        pd[pd_index].us = 0;
        pd[pd_index].ps = 0;
        pd[pd_index].phy_addr_pt = current_pt_physical >> 12;

        /*
         * ====================================================
         * PT -> KERNEL
         * ====================================================
         */

        unsigned long pt_entry = (pt_number * 512) + pt_index;
        pt[pt_entry].p  = 1;
        pt[pt_entry].rw = 1;
        pt[pt_entry].us = 0;
        pt[pt_entry].frames = physical >> 12;

        /*
         * Código do kernel pode executar.
         */

        pt[pt_entry].nx = 0;
    }


     /*
     * ========================================================
     * ARMAZENAR O PRÓXIMO NÚMERO DE TABELA (PT) LIVRE
     * ========================================================
     * 1 = Tabela PT[0] ocupada pela Identidade.
     * Depois, convertemos as páginas do kernel em blocos de tabelas (divisão por 512).
     */
    unsigned long kernel_tables = (kernel_pages + 511) / 512;
    g_next_pt_number += kernel_tables;

     /*
     * ========================================================
     * FRAMEBUFFER (Continuação do seu setup_paging)
     * ========================================================
     *
     * Virtual:  0xFFFF8000E0000000
     * Physical: boot_info->Graphics.FrameBufferBase
     * ========================================================
     */

    framebuffer_phys = boot_info->Graphics.FrameBufferBase;
    framebuffer_size = boot_info->Graphics.FrameBufferSize;

    /*
     * Limitação de segurança: Se o tamanho for maior que 64 MB,
     * mapeia estritamente apenas os primeiros 64 MB.
     */
    if (framebuffer_size > 0x4000000UL)
    {
        framebuffer_size = 0x4000000UL;
    }


    /*
     * ========================================================
     * QUANTIDADE DE PÁGINAS DO FRAMEBUFFER
     * ========================================================
     */

    framebuffer_pages = (framebuffer_size + PAGE_SIZE - 1)/ PAGE_SIZE;

    /*
     * ========================================================
     * PML4[256]
     * ========================================================
     */

    pml4[256].p  = 1;
    pml4[256].rw = 1;
    pml4[256].us = 0;

    pml4[256].phy_addr_pdpt =
        PDPT_256_PHYSICAL >> 12;


    /*
     * ========================================================
     * CALCULAR ÍNDICES DO FRAMEBUFFER
     * ========================================================
     */

    unsigned long framebuffer_virtual =
        KERNEL_VIDEO_VIRTUAL_BASE;


    unsigned long framebuffer_pdpt_index =
        (framebuffer_virtual >> 30) & 0x1FF;


    /*
     * ========================================================
     * PDPT -> PD
     * ========================================================
     */

    pdpt_256[framebuffer_pdpt_index].p  = 1;
    pdpt_256[framebuffer_pdpt_index].rw = 1;
    pdpt_256[framebuffer_pdpt_index].us = 0;
    pdpt_256[framebuffer_pdpt_index].phy_addr_pd = PD_VIDEO_PHYSICAL >> 12;


    /*
     * ========================================================
     * PRIMEIRA PT DISPONÍVEL APÓS O KERNEL
     * ========================================================
     *
     * PT[0] = identity
     *
     * PT[1...] = kernel
     *
     * Portanto o framebuffer começa depois das PTs
     * necessárias para o kernel.
     *
     * ========================================================
     */

    unsigned long framebuffer_pt_start = g_next_pt_number;

    /*
     * ========================================================
     * MAPEAR FRAMEBUFFER (Com PAT / Write-Combining)
     * ========================================================
     */

    for (unsigned long page = 0;
         page < framebuffer_pages;
         page++)
    {
        unsigned long pt_number = framebuffer_pt_start + (page / 512);


        /*
         * Verificar limite das PTs.
         */

        if (pt_number >= NUM_PT_TABLES)
        {
            break;
        }


        /*
         * Endereço físico do framebuffer.
         */

        unsigned long physical = framebuffer_phys + page * PAGE_SIZE;


        /*
         * Endereço virtual.
         */

        unsigned long virtual_addr = KERNEL_VIDEO_VIRTUAL_BASE + page * PAGE_SIZE;


        /*
         * Índice dentro do PD.
         */

        unsigned long pd_index = (virtual_addr >> 21) & 0x1FF;


        /*
         * Índice dentro da PT.
         */

        unsigned long pt_index = (virtual_addr >> 12) & 0x1FF;


        /*
         * Endereço físico da PT.
         */

        unsigned long current_pt_physical =
            PT_PHYSICAL +
            pt_number * PAGE_SIZE;


        /*
         * ====================================================
         * PD -> PT
         * ====================================================
         */

        pd_video[pd_index].p  = 1;
        pd_video[pd_index].rw = 1;
        pd_video[pd_index].us = 0;
        pd_video[pd_index].ps = 0;

        pd_video[pd_index].phy_addr_pt =
            current_pt_physical >> 12;


        /*
         * ====================================================
         * PT -> FRAMEBUFFER
         * ====================================================
         */

        unsigned long pt_entry =
            (pt_number * 512) +
            pt_index;


        pt[pt_entry].p  = 1;
        pt[pt_entry].rw = 1;
        pt[pt_entry].us = 0;

        pt[pt_entry].frames =
            physical >> 12;

        /*
         * ATIVAÇÃO DO WRITE-COMBINING (PAT)
         * PWT = 1, PCD = 1 -> Seleciona a entrada PAT3 na arquitetura x86.
         * Por padrão, a grande maioria dos firmwares configura PAT3 como WC.
         */
        pt[pt_entry].pwt = 1; // Bit 3 da entrada da PT
        pt[pt_entry].pcd = 1; // Bit 4 da entrada da PT

        /*
         * Framebuffer é memória de dados.
         *
         * NX = 1
         */

        pt[pt_entry].nx = 1;

    }

    g_next_pt_number += (framebuffer_pages + 511) / 512;
    /*
     * ========================================================================
     * HABILITAR NX
     *
     * Só fazemos isso depois de construir as tabelas.
     * ========================================================================
     */

    enable_nxe();

    /*
     * ========================================================
     * DESATIVAR INTERRUPÇÕES
     * ========================================================
     */

    __asm__ __volatile__(
        "cli"
    );


    /*
     * ========================================================
     * CARREGAR CR3
     * ========================================================
     *
     * CR3 recebe SEMPRE o endereço físico da PML4.
     *
     * ========================================================
     */

    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(PML4_PHYSICAL)
        : "memory"
    );
}