/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: test.c
 *    Description: Demonstração de leitura de setores em bloco via DMA AHCI
 *                 focado na validação e extração de metadados avançados de
 *                 discos com estrutura GPT (LBA 1).
 * ============================================================================
 */

#include <kernel/klib.h>
#include <kernel/drivers/storage/ahci.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pmm.h>

void test_read_gpt_table(void)
{
    kprintf("[Storage Test] Iniciando teste de leitura GPT por DMA...\n");

    /* 1. Aloca uma página física livre (4KB) na RAM para o DMA */
    uintptr_t target_phys_buffer = pmm_alloc_page();
    if (target_phys_buffer == 0)
    {
        kprintf("[Storage Test] Erro: Falha ao alocar pagina fisica para DMA.\n");
        return;
    }

    /* 
     * 2. Invoca a API do driver de bloco.
     * Na especificação GPT, o cabeçalho principal fica obrigatoriamente no LBA 1.
     * Solicitamos 1 setor (512 bytes).
     */
    int status = ahci_read_blocks(0, 1, 1, target_phys_buffer);

    if (status != 0)
    {
        kprintf("[Storage Test] Erro critico: Falha na transferencia DMA do LBA 1.\n");
        pmm_free_page(target_phys_buffer);
        return;
    }

    kprintf("[Storage Test] DMA do LBA 1 concluido. Mapeando buffer virtual...\n");

    /* 3. Mapeia a página física de dados para um endereço virtual do CPU */
    uint8_t *sector_data = (uint8_t *)vmm_map_device((unsigned long)target_phys_buffer, PAGE_SIZE);
    if (sector_data == NULL)
    {
        kprintf("[Storage Test] Erro: Falha ao mapear virtualmente a pagina de dados.\n");
        pmm_free_page(target_phys_buffer);
        return;
    }

    /* 
     * 4. Valida a assinatura mágica da GPT nos primeiros 8 bytes do LBA 1.
     * O padrão "EFI PART" corresponde a 0x5452415020494645 (Little-Endian).
     */
    uint64_t gpt_signature = *(uint64_t *)(sector_data);
    
    kprintf("[Storage Test] LBA 1 lido. Assinatura de 64-bits: 0x%lx\n", gpt_signature);

    if (gpt_signature == 0x5452415020494645UL)
    {
        kprintf("[Storage Test] SUCESSO: Estrutura GPT (EFI PART) detetada e valida!\n");
        
        /* --- INFORMAÇÕES ADICIONAIS EXTRAÍDAS DO DISCO --- */

        // Revisão da GPT (Offset 8) - Tipicamente 0x00010000 para v1.0
        uint32_t gpt_revision = *(uint32_t *)(sector_data + 8);

        // LBA Atual (Offset 24) - Deve ser 1
        uint64_t current_lba = *(uint64_t *)(sector_data + 24);

        // LBA do Cabeçalho Backup (Offset 32) - Fica no último LBA do disco físico
        uint64_t backup_lba = *(uint64_t *)(sector_data + 32);

        // Primeiro LBA utilizável para dados das partições (Offset 40)
        uint64_t first_usable_lba = *(uint64_t *)(sector_data + 40);

        // Último LBA utilizável para dados (Offset 48)
        uint64_t last_usable_lba = *(uint64_t *)(sector_data + 48);

        // GUID Único do Disco (Offset 56 - 16 bytes)
        uint32_t *disk_guid = (uint32_t *)(sector_data + 56);

        // LBA Inicial do Array de Registos de Partição (Offset 72) - Normalmente LBA 2
        uint64_t partition_array_lba = *(uint64_t *)(sector_data + 72);

        // Número de partições disponíveis e tamanho de cada entrada
        uint32_t num_partitions = *(uint32_t *)(sector_data + 80);
        uint32_t size_partition_entry = *(uint32_t *)(sector_data + 84);

        /* Cálculo da capacidade aproximada informada pelo cabeçalho GPT */
        uint64_t total_sectors_gpt = backup_lba + 1;
        uint64_t total_size_mb = (total_sectors_gpt * 512) / (1024 * 1024);

        /* Print estruturado dos metadados */
        kprintf("\n========================================================\n");
        kprintf("[GPT] Versao da Especificacao:  %d.%d\n", (gpt_revision >> 16), (gpt_revision & 0xFFFF));
        kprintf("[GPT] GUID do Disco:           {%08x-%04x-%04x-%04x%08x}\n", 
                disk_guid[0], (disk_guid[1] & 0xFFFF), (disk_guid[1] >> 16), (disk_guid[2] & 0xFFFF), disk_guid[3]);
        kprintf("[GPT] Capacidade Total (Fisica): %d MB (%ld setores)\n", total_size_mb, total_sectors_gpt);
        kprintf("[GPT] Setor de Backup (Espelho): LBA %ld\n", backup_lba);
        kprintf("[GPT] Intervalo de Dados Util: LBA %ld ate LBA %ld\n", first_usable_lba, last_usable_lba);
        kprintf("[GPT] LBA Inicial das Particoes: LBA %ld\n", partition_array_lba);
        kprintf("[GPT] Tabela: Maximo de %d particoes com blocos de %d bytes.\n", num_partitions, size_partition_entry);
        kprintf("========================================================\n\n");
    }
    else
    {
        kprintf("[Storage Test] FALHA: O disco nao utiliza o particionamento GPT.\n");
    }

    /* 5. Limpeza de recursos após o uso */
    // vmm_unmap_device(sector_data, PAGE_SIZE); 
    pmm_free_page(target_phys_buffer);

    kprintf("[Storage Test] Fim do teste. Sistema em halt controlado.\n");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
