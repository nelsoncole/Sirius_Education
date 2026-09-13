/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: partitions.c
 *    Description: Analisador síncrono de tabelas MBR/GPT e gerador de volumes.
 *                 Usa pool_alloc e pool_free com passagem explícita de tamanho
 *                 para garantir buffers alinhados por DMA (4KB).
 * 
 *         Author: Nelson Cole
 *   Created Date: 12/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 13/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/storage/partitions.h>
#include <kernel/klib.h>

//-----------------------------------------------------------------------------
// OPERAÇÃO VIRTUAL DE INTERCEPÇÃO DE E/S
//-----------------------------------------------------------------------------
static int partition_read_blocks(block_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    partition_ctx_t* ctx = (partition_ctx_t*)dev->private_data;

    // Proteção rigorosa: Impede que o driver de FS leia além do limite da partição
    if (lba + count > ctx->total_sectors) {
        return -1; 
    }

    // A MAGIA: Adicionamos o LBA inicial da partição ao pedido original e encaminha para o hardware real
    return ctx->phys_dev->read_blocks(ctx->phys_dev, ctx->start_lba + lba, count, buffer);
}

static int partition_write_blocks(block_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    partition_ctx_t* ctx = (partition_ctx_t*)dev->private_data;

    if (lba + count > ctx->total_sectors) {
        return -1;
    }

    // Tradução idêntica para escrita segura
    return ctx->phys_dev->write_blocks(ctx->phys_dev, ctx->start_lba + lba, count, buffer);
}

//-----------------------------------------------------------------------------
// AUXILIAR DE REGISTO DE PARTIÇÃO VIRTUAL
//-----------------------------------------------------------------------------
static void create_virtual_partition(block_device_t* phys_dev, const char* part_name, uint64_t start_lba, uint64_t total_sectors) {
    // 1. Aloca as estruturas de controlo lógicas na Heap (Não exigem alinhamento DMA)
    block_device_t* virt_dev = (block_device_t*)kmalloc(sizeof(block_device_t));
    partition_ctx_t* ctx = (partition_ctx_t*)kmalloc(sizeof(partition_ctx_t));
    
    if (!virt_dev || !ctx) {
        if (virt_dev) kfree(virt_dev);
        if (ctx) kfree(ctx);
        return;
    }

    // 2. Define as propriedades do contexto interno
    ctx->phys_dev = phys_dev;
    ctx->start_lba = start_lba;
    ctx->total_sectors = total_sectors;

    // 3. Preenche a interface abstrata compatível com o block.h e o FAT32
    memset(virt_dev, 0, sizeof(block_device_t));
    ksprintf(virt_dev->name, "%s", part_name);
    virt_dev->total_sectors = total_sectors;
    virt_dev->sector_size   = phys_dev->sector_size;
    virt_dev->read_blocks   = partition_read_blocks;  
    virt_dev->write_blocks  = partition_write_blocks; 
    virt_dev->ioctl         = phys_dev->ioctl;
    virt_dev->private_data  = ctx;                    

    // 4. Regista no seu catálogo global (block.c)
    int p_id = register_block_device(virt_dev);
    if (p_id >= 0) {
        kprintf("[PARTITION] Partição '%s' [LBA %lld - %lld] registada com ID %d.\n", 
                virt_dev->name, start_lba, start_lba + total_sectors - 1, p_id);
    } else {
        kfree(ctx);
        kfree(virt_dev);
    }
}

//-----------------------------------------------------------------------------
// PARSERS GERAIS (SCANNER PRINCIPAL)
//-----------------------------------------------------------------------------
void partition_scan_device(block_device_t* phys_dev) {
    if (!phys_dev || !phys_dev->read_blocks) return;

    uint32_t sector_size = phys_dev->sector_size;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para o LBA bruto
    uint8_t* buffer = (uint8_t*)pool_alloc(sector_size);
    if (!buffer) return;

    // 1. Lê o LBA 0 (Setor de Boot MBR)
    if (phys_dev->read_blocks(phys_dev, 0, 1, buffer) != 0) {
        pool_free(buffer, sector_size); // CORRIGIDO: Passagem de tamanho
        return;
    }

    // Valida a assinatura de boot universal (0xAA55) na MBR
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) {
        kprintf("[PARTITION] Dispositivo '%s' não possui assinatura de partição válida.\n", phys_dev->name);
        pool_free(buffer, sector_size); // CORRIGIDO: Passagem de tamanho
        return;
    }

    mbr_entry_t* mbr_table = (mbr_entry_t*)&buffer[0x1BE];
    int is_gpt_protective = 0;

    // 2. Varre a MBR à procura do tipo 0xEE (Sinalizador de GPT ativa)
    for (int i = 0; i < 4; i++) {
        if (mbr_table[i].partition_type == 0xEE) {
            is_gpt_protective = 1;
            break;
        }
    }

    //-----------------------------------------------------
    // FLUXO DE PROCESSAMENTO GPT
    //-----------------------------------------------------
    if (is_gpt_protective) {
        // Lê o LBA 1 (Cabeçalho GPT) para dentro do buffer alinhado via DMA
        if (phys_dev->read_blocks(phys_dev, 1, 1, buffer) == 0) {
            gpt_header_t* gpt = (gpt_header_t*)buffer;
            
            // Verifica a assinatura mágica "EFI PART"
            if (memcmp(gpt->signature, "EFI PART", 8) == 0) {
                uint32_t entry_size = gpt->size_partition_entry;
                uint32_t total_entries = gpt->num_partition_entries;
                uint64_t entries_lba = gpt->partition_entries_lba;
                
                // Mitigação de segurança contra dados corrompidos
                if (total_entries > 128) total_entries = 128;

                // Calcula quantos setores a tabela de partições ocupa
                uint32_t entries_sectors = (total_entries * entry_size + sector_size - 1) / sector_size;
                uint32_t entries_alloc_size = entries_sectors * sector_size;

                // ALINHAMENTO DMA OBRIGATÓRIO: Aloca a tabela de entradas GPT do Pool alinhado
                uint8_t* entries_buf = (uint8_t*)pool_alloc(entries_alloc_size);
                
                if (entries_buf && phys_dev->read_blocks(phys_dev, entries_lba, entries_sectors, entries_buf) == 0) {
                    int part_index = 1;
                    
                    for (uint32_t i = 0; i < total_entries; i++) {
                        gpt_entry_t* entry = (gpt_entry_t*)(entries_buf + (i * entry_size));
                        
                        // Se o GUID do tipo for diferente de zero, a partição é válida
                        static const uint8_t zero_guid[16] = {0};
                        if (memcmp(entry->partition_type_guid, zero_guid, 16) != 0) {
                            char part_name[32];
                            ksprintf(part_name, "%s.%d", phys_dev->name, part_index++);
                            
                            uint64_t size = (entry->ending_lba - entry->starting_lba) + 1;
                            create_virtual_partition(phys_dev, part_name, entry->starting_lba, size);
                        }
                    }
                }
                // Libertação correta do buffer alinhado das entradas passando o tamanho exato alocado
                if (entries_buf) {
                    pool_free(entries_buf, entries_alloc_size); // CORRIGIDO: Passagem de tamanho
                }
            }
        }
    } 
    //-----------------------------------------------------
    // FLUXO DE PROCESSAMENTO MBR CLÁSSICO
    //-----------------------------------------------------
    else {
        int part_index = 1;
        for (int i = 0; i < 4; i++) {
            if (mbr_table[i].partition_type != 0x00 && mbr_table[i].total_sectors > 0) {
                char part_name[32];
                ksprintf(part_name, "%s.%d", phys_dev->name, part_index++);
                
                create_virtual_partition(phys_dev, part_name, mbr_table[i].start_lba, mbr_table[i].total_sectors);
            }
        }
    }

    // pool_free com o tamanho exato do setor do hardware
    pool_free(buffer, sector_size);
}
