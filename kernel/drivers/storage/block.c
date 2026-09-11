/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: block.c
 *    Description: Implementação do Subsistema de Dispositivos de Bloco.
 *                 Gere o catálogo global de unidades de armazenamento registadas
 *                 e fornece interfaces de pesquisa para o Kernel e VFS.
 * 
 *         Author: Nelson Cole
 *   Created Date: 11/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 11/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/storage/block.h>
#include <kernel/klib.h>

/* Tabela estática global para registo de dispositivos */
static block_device_t* g_block_devices[BLOCK_MAX_DEVICES];
static uint32_t g_next_device_id = 0;

/**
 * Inicializa o subsistema de controlo e tabelas de dispositivos de bloco.
 */
void block_subsystem_init(void) {
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        g_block_devices[i] = NULL;
    }
    g_next_device_id = 0;
}

/**
 * Regista um dispositivo de blocos ativo no catálogo global do Kernel.
 */
int register_block_device(block_device_t* dev) {
    if (!dev) {
        return -1; // Ponteiro inválido
    }

    // Encontra um slot livre na tabela global
    int free_slot = -1;
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        if (g_block_devices[i] == NULL) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        return -2; // Tabela cheia (BLOCK_MAX_DEVICES atingido)
    }

    // Atribui as propriedades de identificação do subsistema ao dispositivo
    dev->id = g_next_device_id++;
    g_block_devices[free_slot] = dev;

    return dev->id; // Retorna o ID único gerado com sucesso
}

/**
 * Remove o registo de um dispositivo do catálogo global.
 */
int unregister_block_device(uint32_t id) {
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        if (g_block_devices[i] != NULL && g_block_devices[i]->id == id) {
            g_block_devices[i] = NULL;
            return 0; // Removido com sucesso
        }
    }
    return -1; // Dispositivo não encontrado
}

/**
 * Procura um dispositivo de blocos registado pelo seu ID único.
 */
block_device_t* block_get_device(uint32_t id) {
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        if (g_block_devices[i] != NULL && g_block_devices[i]->id == id) {
            return g_block_devices[i];
        }
    }
    return NULL; // Não encontrado
}

/**
 * Procura um dispositivo de blocos registado pelo seu nome literal.
 */
block_device_t* block_get_device_by_name(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        if (g_block_devices[i] != NULL) {
            if (strcmp(g_block_devices[i]->name, name) == 0) {
                return g_block_devices[i];
            }
        }
    }
    return NULL; // Não encontrado
}


/**
 * Varre o catálogo global e imprime no terminal a lista de todos os
 * dispositivos de bloco ativos, com conversão dinâmica para MB, GB ou TB.
 */
void block_list_devices(void) {
    kprintf("\n========================================================================\n");
    kprintf("                       DISPOSITIVOS DE BLOCO CATALOGADOS                \n");
    kprintf("========================================================================\n");
    kprintf(" ID  | Nome         | Tamanho (Sectores) | Setor    | Capacidade Real   \n");
    kprintf("-----+--------------+--------------------+----------+-------------------\n");

    int count = 0;
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        if (g_block_devices[i] != NULL) {
            block_device_t* dev = g_block_devices[i];
            
            // 1. Calcula o tamanho total bruto em Bytes (Evita overflow usando cast explícito de 64-bits)
            uint64_t total_bytes = (uint64_t)dev->total_sectors * dev->sector_size;
            
            // 2. Determina a unidade apropriada e calcula a parte inteira e decimal (.X)
            const char* unit = "Bytes";
            uint64_t integer_part = total_bytes;
            uint64_t decimal_part = 0;

            if (total_bytes >= (1ULL << 40)) { // Maior ou igual a 1 TB
                integer_part = total_bytes / (1ULL << 40);
                decimal_part = ((total_bytes % (1ULL << 40)) * 10) / (1ULL << 40);
                unit = "TB";
            } 
            else if (total_bytes >= (1ULL << 30)) { // Maior ou igual a 1 GB
                integer_part = total_bytes / (1ULL << 30);
                decimal_part = ((total_bytes % (1ULL << 30)) * 10) / (1ULL << 30);
                unit = "GB";
            } 
            else if (total_bytes >= (1ULL << 20)) { // Maior ou igual a 1 MB
                integer_part = total_bytes / (1ULL << 20);
                decimal_part = ((total_bytes % (1ULL << 20)) * 10) / (1ULL << 20);
                unit = "MB";
            } 
            else if (total_bytes >= 1024) { // Maior ou igual a 1 KB
                integer_part = total_bytes / 1024;
                decimal_part = ((total_bytes % 1024) * 10) / 1024;
                unit = "KB";
            }

            // 3. Cria uma string temporária local para conter o tamanho formatado (ex: "120.4 GB")
            char size_str[32];
            if (total_bytes < 1024) {
                ksprintf(size_str, "%llu %s", integer_part, unit);
            } else {
                ksprintf(size_str, "%llu.%llu %s", integer_part, decimal_part, unit);
            }

            // 4. Imprime a linha com alinhamento perfeito usando o novo suporte ao '%-s'
            kprintf(" %02d  | %-12s | %-18llu | %d Bytes | %-17s\n", 
                    dev->id, 
                    dev->name, 
                    dev->total_sectors, 
                    dev->sector_size,
                    size_str);
            count++;
        }
    }

    if (count == 0) {
        kprintf(" [Nenhum dispositivo de blocos foi registado]\n");
    }
    kprintf("========================================================================\n\n");
}
