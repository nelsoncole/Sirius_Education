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
#include <kernel/drivers/storage/block.h>
#include <kernel/kernel/mm/pool.h>

void test_read_gpt_table(void)
{
    block_list_devices();

    kprintf("[Storage Test] Iniciando teste de leitura GPT por DMA...\n");

    /* 1. Aloca uma página física livre (4KB) na RAM para o DMA */
    size_t memory_size = 0x1000;
    uint8_t *sector_data = pool_alloc(memory_size);
    if (sector_data == NULL)
    {
        kprintf("[Storage Test] Erro: Falha ao alocar memoria.\n");
        while (1) {
            __asm__ __volatile__("hlt");
        }
    }
    

    // Localiza o dispositivo de blocos registado pelo AHCI através do nome literal
    //block_device_t* disco = block_get_device_by_name("ahci0");
    block_device_t* disco = block_get_device(0);
    if (!disco) {
        kprintf("[ERRO] Dispositivo 'ahci0' não encontrado no catálogo global.\n");
        return;
    }

    /* 
     * 2. Invoca a API do driver de bloco.
     * Na especificação GPT, o cabeçalho principal fica obrigatoriamente no LBA 1.
     * Solicitamos 1 setor (512 bytes).
     */
    int status = disco->read_blocks(disco, 1, 1, sector_data);

    if (status != 0)
    {
        kprintf("[Storage Test] Erro critico: Falha na transferencia DMA do LBA 1.\n");
        pool_free(sector_data, memory_size);
        while (1) {
            __asm__ __volatile__("hlt");
        }
    }

    kprintf("[Storage Test] DMA do LBA 1 concluido. Mapeando buffer virtual...\n");


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
    pool_free(sector_data, memory_size);

    kprintf("[Storage Test] Fim do teste. Sistema em halt controlado.\n");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}



void test_pool_reusability(void)
{
    kprintf("\n--- [Pool Test] Verificação de Reutilização de Endereço ---\n");

    // 1. Primeira Alocação
    kprintf("[Pool] 1. Alocando primeira pagina...\n");
    void* addr1 = pool_alloc(PAGE_SIZE);
    kprintf("[Pool] Endereco 1: 0x%lx\n", (unsigned long)addr1);

    if (addr1 == NULL) {
        kprintf("[Pool] ERRO: Falha na alocacao inicial.\n");
        while (1) {
            __asm__ __volatile__("hlt");
        }
    }

    // 2. Libertação da Primeira Alocação
    kprintf("[Pool] 2. Liberando primeira pagina (free_pool)...\n");
    pool_free(addr1, PAGE_SIZE);

    // 3. Segunda Alocação (Deverá reciclar o mesmo bit do bitmap se o free_pool funcionou)
    kprintf("[Pool] 3. Alocando segunda pagina imediatamente...\n");
    void* addr2 = pool_alloc(PAGE_SIZE);
    kprintf("[Pool] Endereco 2: 0x%lx\n", (unsigned long)addr2);

    if (addr2 == NULL) {
        kprintf("[Pool] ERRO: Falha na segunda alocacao.\n");
        while (1) {
            __asm__ __volatile__("hlt");
        }
    }

    // 4. Comparação e Validação do Resultado
    if (addr1 == addr2) {
        kprintf("[Pool] SUCESSO: O endereco foi perfeitamente reciclado! (0x%lx == 0x%lx)\n", 
                (unsigned long)addr1, (unsigned long)addr2);
    } else {
        kprintf("[Pool] AVISO/FALHA: O bitmap nao reciclou o espaco virtual libertado! (Enderecos diferentes).\n");
    }

    // Limpeza final do teste
    pool_free(addr2, PAGE_SIZE);
    kprintf("--- [Pool Test] Fim do teste de reciclagem ---\n\n");

    while (1) {
        __asm__ __volatile__("hlt");
    }
}