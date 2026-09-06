/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: acpi.c
 *    Description: Analisador ACPI de 64-bits escalável. Consome a janela
 *                 de MMIO gigante para mapear o RSDP e a árvore XSDT.
 *
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 31/08/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/bus/acpi.h>
#include <kernel/kernel/mm/memory_map.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/lib/stdio.h>
#include <kernel/lib/string.h>

// Ponteiro global para a raiz das tabelas (Extended System Description Table)
static acpi_sdt_header_t *g_xsdt = 0;

// Define a variável global
unsigned int g_acpi_pm_timer_port = 0;

/*
 * Validação de Checksum matemática padrão do ACPI
 */
static int acpi_validate_checksum(void *addr, unsigned int length)
{
    unsigned char *bytes = (unsigned char *)addr;
    unsigned char sum = 0;
    for (unsigned int i = 0; i < length; i++)
    {
        sum += bytes[i];
    }
    return (sum == 0);
}

/*
 * Varre a árvore XSDT e mapeia dinamicamente a tabela solicitada via VMM
 */
void *acpi_find_table(const char *signature)
{
    if (!g_xsdt)
        return 0;

    // Calcula a quantidade de ponteiros de 64-bits presentes no corpo da XSDT
    unsigned int entries = (g_xsdt->length - sizeof(acpi_sdt_header_t)) / 8;

    // Os ponteiros físicos iniciam-se logo após o cabeçalho base da XSDT
    unsigned long *table_pointers = (unsigned long *)((unsigned long)g_xsdt + sizeof(acpi_sdt_header_t));

    for (unsigned int i = 0; i < entries; i++)
    {
        unsigned long table_phys = table_pointers[i];
        if (table_phys == 0)
            continue;

        /*
         * MAPEAR PRIMEIRO CABEÇALHO DA SUB-TABELA
         * Mapeamos inicialmente apenas 1 página (4 KB) para ler com segurança a assinatura.
         */
        acpi_sdt_header_t *candidate = (acpi_sdt_header_t *)vmm_map_device(table_phys, sizeof(acpi_sdt_header_t));

        // Verifica se os 4 caracteres da assinatura coincidem
        if (memcmp(candidate->signature, signature, 4) == 0)
        {
            /*
             * MAPEAR TABELA COMPLETA DINAMICAMENTE
             * Agora que sabemos o tamanho real contido no cabeçalho (candidate->length),
             * remapeamos o bloco inteiro de forma contígua na janela de MMIO.
             */
            void *final_table_address = vmm_map_device(table_phys, candidate->length);

            kprintf("[ACPI] Tabela '%s' mapeada com sucesso em virtual: 0x%lX\n",
                    signature, (unsigned long)final_table_address);

            return final_table_address;
        }
    }

    kprintf("[ACPI AVISO] Tabela '%s' nao foi localizada no barramento XSDT.\n", signature);
    return 0;
}

/*
 * Inicializa o subsistema ACPI efetuando o mapeamento do RSDP e da XSDT raiz
 */
void acpi_init(BOOT_INFO *boot_info)
{
    kprintf("[ACPI] Inicializando parser atraves do RSDP do UEFI...\n");

    if (boot_info->RsdpAddress == 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] O Loader UEFI nao transmitiu o endereco do ACPI!\n");
        for (;;);
    }

    /*
     * 1. MAPEAR O RSDP NA JANELA DE HARDWARE
     * Passamos o tamanho exato da estrutura v2 de 64 bits.
     */
    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)vmm_map_device(boot_info->RsdpAddress, sizeof(acpi_rsdp_t));

    // Valida a assinatura de 8 bytes na memória RAM
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] Assinatura 'RSD PTR ' corrompida ou invalida!\n");
        for (;;);
    }

    kprintf("[ACPI] RSDP v%d validado com sucesso em 0x%lX (OEM: %.6s).\n",
            rsdp->revision, (unsigned long)rsdp, rsdp->oem_id);

    // No modo de 64 bits nativo do UEFI, a revisão do ACPI DEVE ser >= 2 (suporte XSDT)
    if (rsdp->revision < 2 || rsdp->xsdt_address == 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] Hardware obsoleto: Sem suporte a tabelas XSDT de 64-bits!\n");
        for (;;);
    }

    /*
     * 2. MAPEAR A TABELA RAIZ XSDT
     * Mapeamos inicialmente 4 KB para capturar o cabeçalho base de metadados.
     */
    acpi_sdt_header_t *header_tmp = (acpi_sdt_header_t *)vmm_map_device(rsdp->xsdt_address, sizeof(acpi_sdt_header_t));

    /*
     * Mapeamos agora a XSDT inteira baseado no tamanho total real (header_tmp->length)
     * lido do hardware.
     */
    g_xsdt = (acpi_sdt_header_t *)vmm_map_device(rsdp->xsdt_address, header_tmp->length);

    // Validação matemática estrita do Checksum da XSDT completa
    if (!acpi_validate_checksum(g_xsdt, g_xsdt->length))
    {
        kernel_panic("[ACPI ERRO CRITICO] Checksum invalido ou corrompido na tabela raiz XSDT!\n");
        for (;;);
    }

    kprintf("[ACPI] Descobrindo a porta do PM Timer via tabela FADT...\n");

    // Procura pela tabela FADT usando a assinatura padrão "FACP"
    unsigned char *fadt = (unsigned char *)acpi_find_table("FACP");

    if (fadt != 0)
    {
        /*
         * Segundo a especificação ACPI:
         * O campo PM_TMR_BLK (endereço base de portas I/O do PM Timer)
         * reside exatamente no offset 76 da tabela FADT e tem 4 bytes.
         */
        g_acpi_pm_timer_port = *(unsigned int *)(fadt + 76);
        kprintf("[ACPI] PM Timer localizado por hardware na porta: 0x%X\n", g_acpi_pm_timer_port);
    }
    else
    {
        kprintf("[ACPI ERRO] Tabela FADT nao encontrada. Fallback para porta padrao.\n");
        g_acpi_pm_timer_port = 0x408; // Fallback de emergência
    }

    kprintf("[ACPI] Raiz XSDT montada com sucesso em 0x%lX (%u bytes).\n",
            (unsigned long)g_xsdt, g_xsdt->length);
}


/*
 * Método Moderno: Lê o ACPI Power Management Timer (PM Timer).
 * Corre a 3.579545 MHz por especificação de hardware.
 * Suporta corretamente o mapeamento de portas acima de 0xFF via DX.
 */

unsigned int acpi_pm_read(void) {
    unsigned int value;
    // Se o ACPI ainda não tiver rodado ou falhado, evita travar lendo a porta padrão
    unsigned short port = (g_acpi_pm_timer_port != 0) ? (unsigned short)g_acpi_pm_timer_port : 0x408;
    
    __asm__ __volatile__("inl %%dx, %0" : "=a"(value) : "d"(port));
    return value;
}