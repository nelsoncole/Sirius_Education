/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fat32.c
 *    Description: Driver do Sistema de Ficheiros FAT32 para o VFS.
 *
 *         Author: Nelson Cole
 *   Created Date: 12/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 12/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/fat/fat32.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/klib.h>

#define FAT32_EOF 0x0FFFFFF8
#define FAT32_BAD_CLUSTER 0x0FFFFFF7

/* Estrutura de uma entrada de diretório FAT32 Standard (32 Bytes) */
typedef struct 
{
    uint8_t name[11];          /* Nome 8.3 (8 caracteres nome, 3 extensão) */
    uint8_t attr;              /* Atributos (Diretório, Ficheiro, etc) */
    uint8_t nt_res;            /* Reservado para NT */
    uint8_t crt_time_tenth;    /* Milissegundos de criação */
    uint16_t crt_time;         /* Hora de criação */
    uint16_t crt_date;         /* Data de criação */
    uint16_t lst_acc_date;     /* Último acesso */
    uint16_t first_cluster_hi; /* 16 bits superiores do cluster inicial */
    uint16_t wrt_time;         /* Hora da última modificação */
    uint16_t wrt_date;         /* Data da última modificação */
    uint16_t first_cluster_lo; /* 16 bits inferiores do cluster inicial */
    uint32_t file_size;        /* Tamanho real do ficheiro em Bytes */
}__attribute__((packed)) fat32_entry_t;

/* Entrada FAT32 Long File Name (LFN) - 32 bytes */
typedef struct {
    uint8_t  sequence_number; // Ordem da entrada (bit 6 (0x40) ativado se for a última/primeira da sequência)
    char     name1[10];       // Caracteres 1 a 5 do nome (UTF-16LE)
    uint8_t  attributes;      // Sempre 0x0F (ATTR_LFN)
    uint8_t  type;            // Sempre 0x00 para fragmentos de LFN
    uint8_t  checksum;        // Checksum do Short File Name (SFN) associado
    char     name2[12];       // Caracteres 6 a 11 do nome (UTF-16LE)
    uint16_t first_cluster;   // Sempre 0x0000 para retrocompatibilidade
    char     name3[4];        // Caracteres 12 a 13 do nome (UTF-16LE)
} __attribute__((packed)) fat32_lfn_entry_t;


/* Estrutura do BPB estendido para FAT32 (Packed para evitar padding do compilador) */
typedef struct
{
    uint8_t bootjmp[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t table_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t head_side_count;
    uint32_t hidden_sector_count;
    uint32_t total_sectors_32;

    /* FAT32 Extended Fields */
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char system_id[8];
} __attribute__((packed)) fat32_bpb_t;

/* Metadados privados do volume montado */
typedef struct
{
    block_device_t *dev;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t first_fat_sector;
    uint32_t first_data_sector;
    uint32_t root_cluster;
} fat32_volume_t;

//-----------------------------------------------------------------------------
// FUNÇÕES AUXILIARES DE TRADUÇÃO DE ARMAZENAMENTO
//-----------------------------------------------------------------------------

/* Converte o índice de um cluster no setor absoluto de início de dados no HDD/SSD */
static inline uint64_t cluster_to_sector(fat32_volume_t *vol, uint32_t cluster)
{
    return vol->first_data_sector + ((cluster - 2) * vol->sectors_per_cluster);
}

/* Algoritmo oficial de Checksum para validar correspondência SFN / LFN */
static uint8_t fat32_checksum(const uint8_t *sfn)
{
    uint8_t sum = 0;
    for (int i = 11; i > 0; i--)
    {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sfn++;
    }
    return sum;
}

/* Lê o valor da tabela FAT para descobrir qual é o próximo cluster da cadeia */
static uint32_t fat32_get_next_cluster(fat32_volume_t *vol, uint32_t current_cluster)
{
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = vol->first_fat_sector + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para o setor da FAT
    uint32_t *buffer = (uint32_t *)pool_alloc(vol->bytes_per_sector);
    if (!buffer)
        return FAT32_BAD_CLUSTER;

    // Efetua a leitura do bloco usando o buffer seguro alinhado à página
    if (vol->dev->read_blocks(vol->dev, fat_sector, 1, buffer) != 0)
    {
        pool_free(buffer, vol->bytes_per_sector); // Libertação correta com tamanho
        return FAT32_BAD_CLUSTER;
    }

    uint32_t next_cluster = buffer[ent_offset / 4] & 0x0FFFFFFF;
    
    // Libertação correta com o tamanho exato do setor do volume
    pool_free(buffer, vol->bytes_per_sector);

    return next_cluster;
}

/**
 * Converte o formato de nome nativo FAT 8.3 (ex: "KERNEL  ELF") extraído do 
 * disco físico para uma string legível e padronizada (ex: "kernel.elf").
 *
 * NOTA DE ARQUITETURA (Compatibilidade POSIX):
 * A especificação oficial do FAT32 armazena todos os Short File Names (SFN) 
 * estritamente em letras maiúsculas nas entradas de diretório do hardware.
 * Este módulo realiza a conversão dinâmica para minúsculas na memória RAM
 * durante a fase de formatação visual. Isto garante a conformidade com as
 * convenções Unix-like (Case-Sensitive), otimiza a portabilidade de binários
 * compilados e alinha a experiência de navegação do utilizador no ecossistema.
 */
static void fat32_format_name(const uint8_t *raw_name, char *out_name)
{
    int p = 0;
    // Copia o nome (remove espaços à direita)
    for (int i = 0; i < 8; i++)
    {
        if (raw_name[i] != ' ')
            out_name[p++] = raw_name[i];
    }
    // Adiciona o ponto se existir extensão
    if (raw_name[8] != ' ')
    {
        out_name[p++] = '.';
        for (int i = 8; i < 11; i++)
        {
            if (raw_name[i] != ' ')
                out_name[p++] = raw_name[i];
        }
    }
    out_name[p] = '\0';

    // Converte para minúsculas para padronização POSIX
    for (int i = 0; out_name[i]; i++)
    {
        if (out_name[i] >= 'A' && out_name[i] <= 'Z')
            out_name[i] += 32;
    }
}

/* Escreve um valor num índice específico da tabela FAT (Encadeamento) */
static int fat32_set_cluster(fat32_volume_t* vol, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = vol->first_fat_sector + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para o setor da FAT
    uint32_t* buffer = (uint32_t*)pool_alloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    // Lê o setor da FAT usando o buffer seguro alinhado à página
    if (vol->dev->read_blocks(vol->dev, fat_sector, 1, buffer) != 0) {
        pool_free(buffer, vol->bytes_per_sector); // Libertação correta com tamanho
        return -1;
    }

    buffer[ent_offset / 4] = (buffer[ent_offset / 4] & 0xF0000000) | (value & 0x0FFFFFFF);

    // Escreve o setor modificado de volta no HDD/SSD
    if (vol->dev->write_blocks(vol->dev, fat_sector, 1, buffer) != 0) {
        pool_free(buffer, vol->bytes_per_sector); // Libertação correta com tamanho
        return -1;
    }

    // Libertação de sucesso com o tamanho exato do setor do volume
    pool_free(buffer, vol->bytes_per_sector);
    return 0;
}

/* Procura um cluster livre na FAT, marca-o como EOF e retorna o seu índice */
static uint32_t fat32_allocate_cluster(fat32_volume_t* vol) {
    uint32_t sector_size = vol->bytes_per_sector;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para ler os setores da FAT
    uint32_t* buffer = (uint32_t*)pool_alloc(sector_size);
    if (!buffer) return FAT32_BAD_CLUSTER;

    uint32_t total_fat_sectors = vol->first_data_sector - vol->first_fat_sector;
    uint32_t entries_per_sector = sector_size / 4;

    for (uint32_t sector = 0; sector < total_fat_sectors; sector++) {
        if (vol->dev->read_blocks(vol->dev, vol->first_fat_sector + sector, 1, buffer) != 0) {
            pool_free(buffer, sector_size);
            return FAT32_BAD_CLUSTER;
        }

        for (uint32_t i = 0; i < entries_per_sector; i++) {
            // Mapeamento matemático real e absoluto do índice do cluster baseado no setor físico
            uint32_t target_cluster = (sector * entries_per_sector) + i;

            // Clusters 0 e 1 são estritamente reservados no ecossistema FAT (Mídia e Flags)
            if (target_cluster < 2) continue;

            // Se o cluster na tabela estiver livre (0x00000000)
            if ((buffer[i] & 0x0FFFFFFF) == 0) {
                // Liberta primeiro o buffer DMA alocado
                pool_free(buffer, sector_size); 
                
                // Reserva o cluster de forma estrita marcando-o como EOF na tabela FAT
                if (fat32_set_cluster(vol, target_cluster, FAT32_EOF) != 0) {
                    return FAT32_BAD_CLUSTER;
                }
                
                return target_cluster; // Retorna o ID real e sincronizado
            }
        }
    }

    pool_free(buffer, sector_size);
    return FAT32_BAD_CLUSTER; // Disco cheio
}

/* Percorre a tabela FAT e liberta todos os clusters associados a uma cadeia */
static void fat32_free_cluster_chain(fat32_volume_t *vol, uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= FAT32_BAD_CLUSTER) return;

    uint32_t current = start_cluster;
    while (current < FAT32_EOF && current != FAT32_BAD_CLUSTER && current != 0) {
        // Descobre qual é o próximo cluster antes de zerar o atual
        uint32_t next = fat32_get_next_cluster(vol, current);
        
        // Marca o cluster atual como LIVRE (0x00000000) na tabela FAT
        fat32_set_cluster(vol, current, 0x00000000);
        
        current = next;
    }
}

/* Converte o nome standard Unix/POSIX (ex: "teste.txt") de volta para o formato FAT 8.3 ("TESTE   TXT") */
static void fat32_to_83_name(const char* src, uint8_t* dest) {
    memset(dest, ' ', 11);
    int i = 0, d = 0;
    
    // Copia o nome até achar o ponto ou estourar 8 caracteres
    while (src[i] && src[i] != '.' && d < 8) {
        char c = src[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // Uppercase
        dest[d++] = c;
    }
    
    // Se saiu por causa do ponto, avança na string de origem
    if (src[i] == '.') i++;
    else {
        while (src[i] && src[i] != '.') i++; // Pula o resto se passou de 8
        if (src[i] == '.') i++;
    }
    
    // Copia a extensão
    d = 8;
    while (src[i] && d < 11) {
        char c = src[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        dest[d++] = c;
    }
}

/* Insere uma nova cadeia de entradas de diretório (LFN + SFN) num nó pai existente */
static int fat32_add_entry(vfs_node_t* parent, const char* name, uint8_t attr, uint32_t first_cluster, uint32_t size) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;
    uint32_t cluster = parent->inode;
    uint32_t alloc_size = vol->bytes_per_cluster;

    // 1. Gera o nome 8.3 curto usando a sua função e calcula o Checksum correspondente
    uint8_t sfn_name[11];
    fat32_to_83_name(name, sfn_name);
    uint8_t chksum = fat32_checksum(sfn_name);

    // 2. Calcula a quantidade de entradas necessárias (N LFNs + 1 SFN)
    int name_len = strlen(name);
    int lfn_entries_needed = (name_len + 12) / 13;
    int total_entries_needed = lfn_entries_needed + 1;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para o cluster do diretório
    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return -1;

    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER) {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) break;

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);
        
        int contiguous_found = 0;
        uint32_t start_index = 0;

        // 3. Varre o cluster à procura de um espaço contíguo que caiba todas as entradas
        for (uint32_t i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                if (contiguous_found == 0) start_index = i;
                contiguous_found++;

                if (contiguous_found == total_entries_needed) {
                    uint32_t curr_idx = start_index;

                    // A) Grava as entradas LFN em ordem reversa (exigência da especificação VFAT)
                    for (int lfn_idx = lfn_entries_needed - 1; lfn_idx >= 0; lfn_idx--) {
                        fat32_lfn_entry_t* lfn = (fat32_lfn_entry_t*)&entries[curr_idx++];
                        memset(lfn, 0xFF, sizeof(fat32_lfn_entry_t));

                        // O último fragmento físico (primeiro lógico) recebe o bit 0x40
                        lfn->sequence_number = (lfn_idx == lfn_entries_needed - 1) ? ((lfn_idx + 1) | 0x40) : (lfn_idx + 1);
                        lfn->attributes = 0x0F; // ATTR_LFN
                        lfn->type = 0x00;
                        lfn->checksum = chksum;
                        lfn->first_cluster = 0x0000;

                        int char_start = lfn_idx * 13;

                        // Copia name1 (5 caracteres UTF-16LE em formato byte-array)
                        for (int k = 0; k < 5; k++) {
                            int idx = char_start + k;
                            uint16_t wc = (idx < name_len) ? (uint8_t)name[idx] : ((idx == name_len) ? 0x0000 : 0xFFFF);
                            lfn->name1[k * 2] = wc & 0xFF;
                            lfn->name1[k * 2 + 1] = (wc >> 8) & 0xFF;
                        }
                        // Copia name2 (6 caracteres UTF-16LE)
                        for (int k = 0; k < 6; k++) {
                            int idx = char_start + 5 + k;
                            uint16_t wc = (idx < name_len) ? (uint8_t)name[idx] : ((idx == name_len) ? 0x0000 : 0xFFFF);
                            lfn->name2[k * 2] = wc & 0xFF;
                            lfn->name2[k * 2 + 1] = (wc >> 8) & 0xFF;
                        }
                        // Copia name3 (2 caracteres UTF-16LE)
                        for (int k = 0; k < 2; k++) {
                            int idx = char_start + 11 + k;
                            uint16_t wc = (idx < name_len) ? (uint8_t)name[idx] : ((idx == name_len) ? 0x0000 : 0xFFFF);
                            lfn->name3[k * 2] = wc & 0xFF;
                            lfn->name3[k * 2 + 1] = (wc >> 8) & 0xFF;
                        }
                    }

                    // B) Grava a entrada SFN padrão logo a seguir
                    fat32_entry_t* sfn = &entries[curr_idx];
                    memset(sfn, 0, sizeof(fat32_entry_t));
                    memcpy(sfn->name, sfn_name, 11);
                    sfn->attr = attr;
                    sfn->first_cluster_hi = (uint16_t)((first_cluster >> 16) & 0xFFFF);
                    sfn->first_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
                    sfn->file_size = size;

                    // Salva o bloco modificado de volta no HDD/SSD usando o buffer da Pool
                    if (vol->dev->write_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) {
                        pool_free(buf, alloc_size);
                        return -1;
                    }
                    pool_free(buf, alloc_size);
                    return 0; // Sucesso
                }
            } else {
                contiguous_found = 0; // Reinicia se a sequência de espaços livres quebrar
            }
        }

        // Se o cluster atual está cheio, tenta pegar o próximo ou aloca um novo para expandir o diretório
        uint32_t next = fat32_get_next_cluster(vol, cluster);
        if (next >= FAT32_EOF || next == FAT32_BAD_CLUSTER) {
            uint32_t new_cluster = fat32_allocate_cluster(vol);
            if (new_cluster == FAT32_BAD_CLUSTER) break;
            fat32_set_cluster(vol, cluster, new_cluster);
            
            // Limpa o novo bloco de diretório em disco antes de usar
            memset(buf, 0, alloc_size);
            vol->dev->write_blocks(vol->dev, cluster_to_sector(vol, new_cluster), vol->sectors_per_cluster, buf);
            next = new_cluster;
        }
        cluster = next;
    }

    pool_free(buf, alloc_size); // Libertação correta com tamanho em caso de erro/falha
    return -1;
}

/* Atualiza o tamanho de um ficheiro na sua entrada de diretório de 32 bytes correspondente */
static int fat32_update_entry_size(vfs_node_t* parent, const char* name, uint32_t new_size) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;
    uint32_t cluster = parent->inode;
    uint32_t alloc_size = vol->bytes_per_cluster;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para ler o cluster do diretório
    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return -1;

    uint8_t fat_name[11];
    fat32_to_83_name(name, fat_name);

    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER) {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) break;

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) {
                pool_free(buf, alloc_size); // Libertação correta com tamanho
                return -1;
            }
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                entries[i].file_size = new_size;
                if (vol->dev->write_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) {
                    pool_free(buf, alloc_size); // Libertação correta com tamanho
                    return -1;
                }
                pool_free(buf, alloc_size); // Libertação correta com tamanho
                return 0;
            }
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }
    pool_free(buf, alloc_size); // Libertação correta com tamanho em caso de falha de leitura
    return -1;
}

/* Marca todas as entradas contíguas de um ficheiro (LFN + SFN) como apagadas (0xE5) */
static int fat32_invalidate_entries(vfs_node_t* parent, const char* name) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;
    uint32_t cluster = parent->inode;
    uint32_t alloc_size = vol->bytes_per_cluster;

    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return -1;

    uint8_t sfn_target[11];
    fat32_to_83_name(name, sfn_target);

    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER) {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) break;

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5 || entries[i].attr == 0x0F) continue;

            // Encontrou a entrada SFN correspondente
            if (memcmp(entries[i].name, sfn_target, 11) == 0) {
                // 1. Anda para trás apagando os LFNs associados pertencentes a este bloco
                int back = (int)i - 1;
                while (back >= 0 && entries[back].attr == 0x0F) {
                    entries[back].name[0] = 0xE5;
                    back--;
                }

                // 2. Apaga a entrada SFN real
                entries[i].name[0] = 0xE5;

                // Salva o cluster limpo de volta no disco
                vol->dev->write_blocks(vol->dev, sector, vol->sectors_per_cluster, buf);
                pool_free(buf, alloc_size);
                return 0; // Sucesso
            }
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    pool_free(buf, alloc_size);
    return -1; // Não encontrado
}

/* Verifica se um cluster de diretório está completamente vazio (ignorando '.' e '..') */
static int fat32_is_dir_empty(fat32_volume_t* vol, uint32_t dir_cluster) {
    uint32_t alloc_size = vol->bytes_per_cluster;
    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return 0;

    uint32_t cluster = dir_cluster;
    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER) {
        if (vol->dev->read_blocks(vol->dev, cluster_to_sector(vol, cluster), vol->sectors_per_cluster, buf) != 0) break;

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) {
                pool_free(buf, alloc_size);
                return 1;
            }
            if (entries[i].name[0] == 0xE5) continue;
            
            // Ignora os ponteiros de navegação '.' e '..'
            if (entries[i].name[0] == '.') continue;

            // Se achou qualquer outro ficheiro/pasta, o diretório NÃO está vazio
            pool_free(buf, alloc_size);
            return 0;
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    pool_free(buf, alloc_size);
    return 1;
}

//-----------------------------------------------------------------------------
// IMPLEMENTAÇÃO COMPLETA DAS OPERAÇÕES DO VFS
//-----------------------------------------------------------------------------
static int fat32_open(vfs_node_t *node, uint32_t flags) {
    if (!node) return -1;

    // 1. Se o ficheiro for aberto para escrita, mas for um diretório, nega o acesso
    if ((flags & VFS_MODE_WRITE) && (node->flags & VFS_DIRECTORY)) {
        return -2; // Erro: Não se pode escrever diretamente num diretório
    }

    // 2. Se o ficheiro for aberto para escrita, mas o nó for Read-Only no FAT (Atributo 0x01)
    if ((flags & VFS_MODE_WRITE) && (node->permissions & 0x01)) { 
        return -3; // Erro: Permissão de escrita negada no hardware físico
    }

    // 3. Suporte a TRUNCATE (Limpar o ficheiro ao abrir para escrita fresca)
    if ((flags & VFS_MODE_WRITE) && (flags & VFS_MODE_TRUNC)) {
        fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
        
        if (vol && node->inode >= 2) {
            kprintf("[FAT32] TRUNCATE ativo para '%s': Libertando cadeia de clusters (Cluster: %u)...\n", 
                    node->name, node->inode);

            // A) Varre a FAT e liberta fisicamente todos os clusters de dados antigos do arquivo
            fat32_free_cluster_chain(vol, node->inode);

            // B) Sincroniza o tamanho do arquivo para 0 no diretório pai em disco
            // Como no open o pai direto nem sempre está acessível na struct do nó, 
            // simulamos o nó pai raiz ou usamos o seu atualizador de metadados padrão:
            vfs_node_t parent_node;
            memset(&parent_node, 0, sizeof(vfs_node_t));
            parent_node.inode = vol->root_cluster; 
            parent_node.private_data = vol;
            parent_node.flags = VFS_DIRECTORY;

            fat32_update_entry_size(&parent_node, node->name, 0);

            // C) Zera o tamanho também no nó em memória RAM
            node->size = 0;
        }
    }

    kprintf("[FAT32] Ficheiro '%s' (Cluster Inicial: %u) verificado e aberto com sucesso.\n", 
            node->name, node->inode);

    return 0; // Sucesso: Nó validado, limpo (se truncado) e pronto
}

/**
 * Força a sincronização de dados e metadados pendentes do volume FAT32 com o hardware.
 * 
 * @param node O nó de ficheiro ou diretório associado ao volume.
 * @return 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int fat32_flush(vfs_node_t *node) {
    if (!node) return -1;

    // Recupera os metadados privados do volume a partir do nó
    fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
    if (!vol || !vol->dev) {
        return -1;
    }

    kprintf("[FAT32] Executando flush em '%s' - Sincronizando buffers de hardware...\n", node->name);

    // Executa o flush de hardware se o driver de bloco (ex: AHCI) suportar a interface ioctl
    if (vol->dev->ioctl) {
        // Comando 1: Padrão no Sirius_Education para Hardware Disk Sync / Flush
        if (vol->dev->ioctl(vol->dev, 1, 0) != 0) {
            kprintf("[FAT32] Aviso: Falha no comando flush do dispositivo de bloco.\n");
            return -1;
        }
    }

    return 0; // Sucesso: Caches físicas sincronizadas
}

static int fat32_close(vfs_node_t *node) {
    if (!node) return -1;

    // Se for um ficheiro regular
    if (node->flags & VFS_FILE) {
        fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
        
        if (vol) {
            kprintf("[FAT32] A fechar '%s': Sincronizando tamanho (%llu Bytes) com o HDD/SSD...\n", 
                    node->name, node->size);

            // 1. Cria um nó temporário que representa o diretório pai (por agora a raiz do FAT32)
            // para permitir que a função localize a entrada de 32 bytes deste ficheiro.
            vfs_node_t parent_node;
            memset(&parent_node, 0, sizeof(vfs_node_t));
            parent_node.inode = vol->root_cluster; // Assume o cluster raiz como pai provisório
            parent_node.private_data = vol;
            parent_node.flags = VFS_DIRECTORY;

            // 2. Persiste o tamanho real atualizado em memória RAM diretamente nos setores do disco físico
            fat32_update_entry_size(&parent_node, node->name, (uint32_t)node->size);
            
            // 3. Executa o flush de hardware de forma modular utilizando a nova função
            fat32_flush(node);
        }
    }

    return 0; // Sucesso: Dados salvos e descritor fechado no Sirius_Education
}

static int fat32_read(vfs_node_t *node, uint64_t offset, uint32_t size, void *buffer)
{
    fat32_volume_t *vol = (fat32_volume_t *)node->private_data;
    uint32_t alloc_size = vol->bytes_per_cluster;

    // Segurança: se for um ficheiro normal, evita ler além do tamanho real
    if (!(node->flags & VFS_DIRECTORY) && (offset >= node->size))
        return 0;
    if (!(node->flags & VFS_DIRECTORY) && (offset + size > node->size))
    {
        size = node->size - offset;
    }

    uint32_t cluster = node->inode; // Inode armazena o cluster inicial
    uint64_t current_offset = 0;

    // 1. Avança na cadeia de clusters até chegar ao offset desejado
    while (current_offset + alloc_size <= offset)
    {
        cluster = fat32_get_next_cluster(vol, cluster);
        if (cluster >= FAT32_EOF || cluster == FAT32_BAD_CLUSTER)
            return 0;
        current_offset += alloc_size;
    }

    uint32_t bytes_lidos = 0;
    // ALINHAMENTO DMA: Aloca do Pool o buffer para transferência física do cluster
    uint8_t *cluster_buf = (uint8_t *)pool_alloc(alloc_size);
    if (!cluster_buf)
        return -1;

    // 2. Loop de leitura de blocos fragmentados por clusters
    while (bytes_lidos < size)
    {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, cluster_buf) != 0)
        {
            break;
        }

        uint32_t cluster_offset = offset + bytes_lidos - current_offset;
        uint32_t bytes_para_copiar = alloc_size - cluster_offset;
        if (bytes_para_copiar > (size - bytes_lidos))
        {
            bytes_para_copiar = size - bytes_lidos;
        }

        memcpy((uint8_t *)buffer + bytes_lidos, cluster_buf + cluster_offset, bytes_para_copiar);
        bytes_lidos += bytes_para_copiar;

        if (bytes_lidos < size)
        {
            cluster = fat32_get_next_cluster(vol, cluster);
            if (cluster >= FAT32_EOF || cluster == FAT32_BAD_CLUSTER)
                break;
            current_offset += alloc_size;
        }
    }

    pool_free(cluster_buf, alloc_size); // Libertação correta com tamanho
    return bytes_lidos;
}

//-----------------------------------------------------------------------------
// IMPLEMENTAÇÃO DE DIRETÓRIO COM SUPORTE LFN (VFS READDIR & FINDDIR)
//-----------------------------------------------------------------------------

static int fat32_readdir(vfs_node_t *node, uint32_t index, vfs_node_t *out_node)
{
    fat32_volume_t *vol = (fat32_volume_t *)node->private_data;
    uint32_t cluster = node->inode;
    uint32_t alloc_size = vol->bytes_per_cluster;

    // ALINHAMENTO DMA: Aloca do Pool o buffer para ler o cluster do diretório
    uint8_t *buf = (uint8_t *)pool_alloc(alloc_size);
    if (!buf)
        return -1;

    uint32_t currentIndex = 0;

    // Variáveis de estado para reconstrução do LFN (Tamanho máximo oficial = 255 + nulo)
    char lfn_buffer[256];
    memset(lfn_buffer, 0, sizeof(lfn_buffer));
    uint8_t target_checksum = 0;
    int has_lfn = 0;

    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER)
    {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0)
            break;

        fat32_entry_t *entries = (fat32_entry_t *)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < max_entries; i++)
        {
            fat32_entry_t *entry = &entries[i];

            if (entry->name[0] == 0x00)
            { // Fim das entradas do diretório
                pool_free(buf, alloc_size);
                return -1;
            }
            if (entry->name[0] == 0xE5)
            {
                has_lfn = 0; // Descarta restos órfãos de LFN
                continue;    // Entrada apagada (Ignorar)
            }

            // Caso seja uma entrada LFN (VFAT Pseudo-entry)
            if (entry->attr == 0x0F)
            {
                fat32_lfn_entry_t *lfn = (fat32_lfn_entry_t *)entry;
                
                // Se for a última entrada LFN da sequência física (início lógico do nome)
                if (lfn->sequence_number & 0x40)
                {
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                    target_checksum = lfn->checksum;
                    has_lfn = 1;
                }
                else if (has_lfn && lfn->checksum != target_checksum)
                {
                    has_lfn = 0; // Checksum desajustado, descarta
                    continue;
                }

                uint32_t seq = (lfn->sequence_number & 0x1F) - 1; // Base 0
                if (seq < 20) // Proteção de limite (20 * 13 = 260 chars)
                {
                    uint32_t char_offset = seq * 13;
                    
                    // Extrai name1 (5 caracteres UTF-16LE para ASCII simples)
                    for (int k = 0; k < 5 && char_offset < 255; k++) {
                        uint16_t wc = lfn->name1[k * 2] | (lfn->name1[k * 2 + 1] << 8);
                        if (wc == 0x0000 || wc == 0xFFFF) break;
                        lfn_buffer[char_offset++] = (char)(wc & 0x00FF);
                    }
                    // Extrai name2 (6 caracteres UTF-16LE)
                    for (int k = 0; k < 6 && char_offset < 255; k++) {
                        uint16_t wc = lfn->name2[k * 2] | (lfn->name2[k * 2 + 1] << 8);
                        if (wc == 0x0000 || wc == 0xFFFF) break;
                        lfn_buffer[char_offset++] = (char)(wc & 0x00FF);
                    }
                    // Extrai name3 (2 caracteres UTF-16LE)
                    for (int k = 0; k < 2 && char_offset < 255; k++) {
                        uint16_t wc = lfn->name3[k * 2] | (lfn->name3[k * 2 + 1] << 8);
                        if (wc == 0x0000 || wc == 0xFFFF) break;
                        lfn_buffer[char_offset++] = (char)(wc & 0x00FF);
                    }
                }
                continue; // Vai para a próxima entrada de 32 bytes para acumular ou achar o SFN
            }

            // Ignorar rótulos de Volume ID puros no VFS
            if (entry->attr & 0x08)
            {
                has_lfn = 0;
                continue;
            }

            // Chegamos a uma entrada SFN válida. Validamos o LFN associado se houver
            if (has_lfn)
            {
                if (fat32_checksum(entry->name) != target_checksum)
                {
                    has_lfn = 0; // Checksum falhou, descarta nome longo
                }
            }

            // Se o índice lógico coincidir com o requisitado
            if (currentIndex == index)
            {
                if (has_lfn && strlen(lfn_buffer) > 0)
                {
                    // Copia o Nome Longo reconstruído para o nó de saída
                    strncpy(out_node->name, lfn_buffer, sizeof(out_node->name) - 1);
                    out_node->name[sizeof(out_node->name) - 1] = '\0';
                }
                else
                {
                    // Fallback para o nome curto tradicional 8.3
                    fat32_format_name(entry->name, out_node->name);
                }

                // Preenche o nó de saída do VFS com os dados reais mapeados
                out_node->flags = (entry->attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
                out_node->size = entry->file_size;
                out_node->inode = ((uint32_t)entry->first_cluster_hi << 16) | entry->first_cluster_lo;
                
                /* Mapeia os atributos do FAT32 para permissões lógicas do VFS */
                if (entry->attr & 0x01) {
                    out_node->permissions = 0x0155; // Read-Only (Equivalente a r-xr-xr-x)
                } else {
                    out_node->permissions = 0x01FF; // Read-Write (Equivalente a rwxrwxrwx)
                }
                out_node->ops = node->ops; // Propaga a tabela estática FAT32
                out_node->private_data = vol;
                out_node->fs = node->fs;
                out_node->ptr_mount = NULL;

                pool_free(buf, alloc_size); // Libertação de sucesso com tamanho
                return 0; 
            }

            currentIndex++;
            has_lfn = 0; // Limpa o estado para o próximo ficheiro
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    pool_free(buf, alloc_size); // Libertação de encerramento com tamanho
    return -1;
}

static vfs_node_t *fat32_finddir(vfs_node_t *node, const char *name)
{
    vfs_node_t *out_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!out_node)
        return NULL;

    uint32_t index = 0;
    while (fat32_readdir(node, index, out_node) == 0)
    {
        // Agora o strcmp compara diretamente com o nome LFN ou SFN 
        // correto dentro de out_node->name
        if (strcmp(out_node->name, name) == 0)
        {
            return out_node; 
        }
        index++;
    }

    kfree(out_node);
    return NULL; 
}

static int fat32_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
    uint32_t alloc_size = vol->bytes_per_cluster;
    
    if (node->flags & VFS_DIRECTORY) return -1; // Não permite escrita direta de bytes brutos em diretórios

    if (node->inode == 0) {
        uint32_t new_cluster = fat32_allocate_cluster(vol);
        if (new_cluster == FAT32_BAD_CLUSTER) return -1;
        node->inode = new_cluster;
    }

    uint32_t cluster = node->inode;
    uint64_t current_offset = 0;

    // 1. Navega até o cluster correspondente ao offset
    while (current_offset + alloc_size <= offset) {
        uint32_t next = fat32_get_next_cluster(vol, cluster);
        if (next >= FAT32_EOF || next == FAT32_BAD_CLUSTER) {
            next = fat32_allocate_cluster(vol);
            if (next == FAT32_BAD_CLUSTER) return 0;
            fat32_set_cluster(vol, cluster, next);
        }
        cluster = next;
        current_offset += alloc_size;
    }

    uint32_t bytes_escritos = 0;
    // ALINHAMENTO DMA: Aloca do Pool o buffer para modificação síncrona
    uint8_t* cluster_buf = (uint8_t*)pool_alloc(alloc_size);
    if (!cluster_buf) return -1;

    // 2. Loop de escrita
    while (bytes_escritos < size) {
        uint64_t sector = cluster_to_sector(vol, cluster);
        
        // Lê o conteúdo atual para escrita parcial (Read-Modify-Write)
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, cluster_buf) != 0) break;

        uint32_t cluster_offset = (offset + bytes_escritos) - current_offset;
        uint32_t bytes_para_copiar = alloc_size - cluster_offset;
        if (bytes_para_copiar > (size - bytes_escritos)) {
            bytes_para_copiar = size - bytes_escritos;
        }

        memcpy(cluster_buf + cluster_offset, (uint8_t*)buffer + bytes_escritos, bytes_para_copiar);

        if (vol->dev->write_blocks(vol->dev, sector, vol->sectors_per_cluster, cluster_buf) != 0) break;

        bytes_escritos += bytes_para_copiar;

        if (bytes_escritos < size) {
            uint32_t next = fat32_get_next_cluster(vol, cluster);
            if (next >= FAT32_EOF || next == FAT32_BAD_CLUSTER) {
                next = fat32_allocate_cluster(vol);
                if (next == FAT32_BAD_CLUSTER) break;
                fat32_set_cluster(vol, cluster, next);
            }
            cluster = next;
            current_offset += alloc_size;
        }
    }

    pool_free(cluster_buf, alloc_size); // Libertação correta com tamanho

    if (offset + bytes_escritos > node->size) {
        node->size = offset + bytes_escritos;
    }

    return bytes_escritos;
}

static int fat32_mkdir(vfs_node_t* parent, const char* name, uint16_t permissions) {
    (void)permissions;
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;

    // BLOQUEIO DE DUPLICIDADE: 
    // Verifica se o diretório/arquivo com este nome já existe
    vfs_node_t* existing_node = fat32_finddir(parent, name);
    if (existing_node != NULL) {
        kprintf("[FAT32] Erro: Impossível criar pasta '%s'. O nome já existe.\n", name);
        kfree(existing_node); // Liberta a memória alocada pelo finddir
        return -1; // Retorna erro (Equivalente POSIX a -EEXIST)
    }

    uint32_t alloc_size = vol->bytes_per_cluster;

    // 1. Aloca um cluster livre para conter os registos do novo diretório
    uint32_t new_cluster = fat32_allocate_cluster(vol);
    if (new_cluster == FAT32_BAD_CLUSTER) return -1;

       // 2. ALINHAMENTO DMA: Aloca do Pool o buffer para inicializar o cluster de subpasta
    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return -1;
    
    // Força a limpeza absoluta de TODO o cluster com zeros. 
    // Isso garante que todas as outras entradas (da 2 até ao fim do cluster) fiquem como 0x00 (Vazias).
    memset(buf, 0, alloc_size);

    fat32_entry_t* dot_entries = (fat32_entry_t*)buf;
    
    // Inicializa a Entrada '.' limpando-a primeiro por segurança
    memset(&dot_entries[0], 0, sizeof(fat32_entry_t));
    memset(dot_entries[0].name, ' ', 11);
    dot_entries[0].name[0] = '.';
    dot_entries[0].attr = 0x10; // Atributo Diretório
    dot_entries[0].first_cluster_hi = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    dot_entries[0].first_cluster_lo = (uint16_t)(new_cluster & 0xFFFF);

    // Inicializa a Entrada '..' limpando-a primeiro por segurança
    memset(&dot_entries[1], 0, sizeof(fat32_entry_t));
    memset(dot_entries[1].name, ' ', 11);
    dot_entries[1].name[0] = '.';
    dot_entries[1].name[1] = '.';
    dot_entries[1].attr = 0x10;
    
    uint32_t parent_cluster = (parent->inode == vol->root_cluster) ? 0 : parent->inode;
    dot_entries[1].first_cluster_hi = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    dot_entries[1].first_cluster_lo = (uint16_t)(parent_cluster & 0xFFFF);

    // Força explicitamente a entrada seguinte (índice 2) a ser 0x00, 
    // indicando o fim real dos arquivos nesta pasta.
    dot_entries[2].name[0] = 0x00;

    // Grava as novas entradas base no disco usando o buffer da Pool
    if (vol->dev->write_blocks(vol->dev, cluster_to_sector(vol, new_cluster), vol->sectors_per_cluster, buf) != 0) {
        pool_free(buf, alloc_size);
        return -1;
    }
    pool_free(buf, alloc_size);

    // 3. Adiciona a referência do diretório recém-criado na lista de entradas do diretório pai
    return fat32_add_entry(parent, name, 0x10, new_cluster, 0);
}

static int fat32_create(vfs_node_t* parent, const char* name, uint16_t permissions) {
    (void)permissions;
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;

    // BLOQUEIO DE DUPLICIDADE: 
    // Impede a criação de arquivos duplicados no mesmo diretório
    vfs_node_t* existing_node = fat32_finddir(parent, name);
    if (existing_node != NULL) {
        kprintf("[FAT32] Erro: Impossível criar arquivo '%s'. O nome já existe.\n", name);
        kfree(existing_node);
        return -1; // Retorna erro
    }

    // 1. Aloca um cluster inicial para o ficheiro vazio
    uint32_t new_cluster = fat32_allocate_cluster(vol);
    if (new_cluster == FAT32_BAD_CLUSTER) return -1;

    // 2. Adiciona a entrada do tipo ficheiro regular (Atributo 0x20 Archive) no diretório pai
    // O fat32_add_entry interno já foi migrado para pool_alloc, garantindo a segurança DMA.
    return fat32_add_entry(parent, name, 0x20, new_cluster, 0);
}

/* Elimina um ficheiro regular do disco e liberta os seus clusters */
static int fat32_unlink(vfs_node_t* parent, const char* name) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;

    // 1. Procura o nó para extrair o cluster inicial (Inode)
    vfs_node_t* node = fat32_finddir(parent, name);
    if (!node) return -1; // Não existe

    if (node->flags & VFS_DIRECTORY) {
        kfree(node);
        return -2; // Erro: unlink só remove ficheiros normais, use rmdir
    }

    // 2. Liberta a corrente de clusters na tabela FAT
    fat32_free_cluster_chain(vol, node->inode);

    // 3. Invalida as entradas físicas (LFN + SFN) no diretório pai
    int res = fat32_invalidate_entries(parent, name);
    
    kfree(node);
    return res;
}

/* Elimina uma subpasta vazia do disco e liberta o seu cluster */
static int fat32_rmdir(vfs_node_t* parent, const char* name) {
    fat32_volume_t* vol = (fat32_volume_t*)parent->private_data;

    vfs_node_t* node = fat32_finddir(parent, name);
    if (!node) return -1;

    if (!(node->flags & VFS_DIRECTORY)) {
        kfree(node);
        return -2; // Erro: Não é um diretório
    }

    // Proteção de sanidade: Impede a remoção das referências cíclicas . e ..
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        kfree(node);
        return -3;
    }

    // 1. Verifica se a pasta está vazia (requisito de segurança POSIX)
    if (!fat32_is_dir_empty(vol, node->inode)) {
        kprintf("[FAT32] Erro: Pasta '%s' nao esta vazia.\n", name);
        kfree(node);
        return -4;
    }

    // 2. Liberta o cluster de registos da subpasta na FAT
    fat32_free_cluster_chain(vol, node->inode);

    // 3. Invalida os slots (LFN + SFN) no diretório pai
    int res = fat32_invalidate_entries(parent, name);

    kfree(node);
    return res;
}

/* Altera permissões lógicas de hardware (Atributo Read-Only do FAT32) */
static int fat32_chmod(vfs_node_t* node, uint16_t mode) {
    fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
    uint32_t alloc_size = vol->bytes_per_cluster;

    uint8_t* buf = (uint8_t*)pool_alloc(alloc_size);
    if (!buf) return -1;

    // Para fins de simplificação, assumimos o nó pai provisório (raiz) para caçar o SFN
    uint32_t cluster = vol->root_cluster; 
    uint8_t sfn_target[11];
    fat32_to_83_name(node->name, sfn_target);

    while (cluster < FAT32_EOF && cluster != FAT32_BAD_CLUSTER) {
        uint64_t sector = cluster_to_sector(vol, cluster);
        if (vol->dev->read_blocks(vol->dev, sector, vol->sectors_per_cluster, buf) != 0) break;

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        uint32_t max_entries = alloc_size / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5 || entries[i].attr == 0x0F) continue;

            if (memcmp(entries[i].name, sfn_target, 11) == 0) {
                // Altera o atributo Read-Only (0x01) baseado nas flags de escrita POSIX
                if (!(mode & 0x0080)) { // Se não tiver a permissão S_IWUSR (Escrita)
                    entries[i].attr |= 0x01; // Ativa Read-Only no hardware
                    node->permissions = 0x0155;
                } else {
                    entries[i].attr &= ~0x01; // Desativa Read-Only
                    node->permissions = 0x01FF;
                }

                vol->dev->write_blocks(vol->dev, sector, vol->sectors_per_cluster, buf);
                pool_free(buf, alloc_size);
                return 0;
            }
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    pool_free(buf, alloc_size);
    return -1;
}

/* Renomeia ou move um arquivo alterando apenas os metadados das tabelas de diretório */
static int fat32_rename(vfs_node_t* parent, const char* old_name, const char* new_name) {
    // 1. BLOQUEIO DE DUPLICIDADE: Verifica se o novo nome já existe no diretório pai
    vfs_node_t* check_exist = fat32_finddir(parent, new_name);
    if (check_exist != NULL) {
        kprintf("[FAT32 RENAME] Erro: O nome destino '%s' já existe.\n", new_name);
        kfree(check_exist); // Liberta a memória do nó encontrado no lookup
        return -3;          // Retorna erro de alvo já existente (Equivalente POSIX a -EEXIST)
    }

    // Localiza os metadados do nó antigo para roubar o cluster inicial e os atributos
    vfs_node_t* old_node = fat32_finddir(parent, old_name);
    if (!old_node) return -1;

    // 2. Adiciona uma cópia idêntica apontando para o mesmo cluster inicial com o NOVO nome
    uint8_t old_attr = (old_node->flags & VFS_DIRECTORY) ? 0x10 : 0x20;
    if (fat32_add_entry(parent, new_name, old_attr, old_node->inode, (uint32_t)old_node->size) != 0) {
        kfree(old_node);
        return -2;
    }

    // 3. Invalida e remove a entrada antiga (LFN + SFN) do disco rígido
    fat32_invalidate_entries(parent, old_name);

    kfree(old_node);
    return 0; // Renomeado nativamente em milissegundos!
}

/**
 * Extrai os metadados nativos de um ficheiro ou diretório do FAT32 e preenche a estrutura do VFS.
 * 
 * @param node O nó do VFS do qual pretendemos ler os metadados.
 * @param buf  O buffer da estrutura stat de saída que será preenchido.
 * @return 0 em caso de sucesso, ou -1 em caso de erro.
 */
static int fat32_stat(vfs_node_t* node, struct vfs_stat* buf) {
    if (!node || !buf) return -1;

    fat32_volume_t* vol = (fat32_volume_t*)node->private_data;
    if (!vol) return -1;

    kprintf("[FAT32] A recolher metadados e estatísticas de '%s'...\n", node->name);

    // 1. O número do Inode no ecossistema FAT32 mapeia estritamente para o Cluster Inicial
    buf->st_ino = node->inode;
    
    // 2. Transpõe o tamanho físico real em Bytes guardado no nó em cache
    buf->st_size = node->size;
    
    // 3. Mapeia o modo lógico (Tipo e Permissões baseadas nas flags do VFS)
    buf->st_mode = node->permissions;
    if (node->flags & VFS_DIRECTORY) {
        buf->st_mode |= 0x4000; // Máscara POSIX S_IFDIR para marcar como Diretório
    } else {
        buf->st_mode |= 0x8000; // Máscara POSIX S_IFREG para marcar como Ficheiro Regular
    }

    // 4. O FAT32 não suporta Donos/Grupos Unix nativos; fixa como root (0) por compatibilidade
    buf->st_uid = 0;
    buf->st_gid = 0;

    // 5. Preserva os atributos brutos de hardware (Hidden, System, Archive, Read-Only)
    // Infere o atributo Read-Only (0x01) com base nas permissões em cache do nó
    buf->st_attr = (node->permissions == 0x0155) ? 0x01 : 0x00;
    
    // Se for uma subpasta, garante que o atributo de Diretório (0x10) está presente
    if (node->flags & VFS_DIRECTORY) {
        buf->st_attr |= 0x10;
    } else {
        buf->st_attr |= 0x20; // Atributo padrão Archive (0x20) para ficheiros normais
    }

    return 0; // Sucesso: Estrutura stat preenchida de forma fidedigna
}

/* ============================================================================
 *        Tabela estática de operações FAT32 vinculada perfeitamente ao VFS
 * ============================================================================ */
static vfs_operations_t g_fat32_ops = {
    .open    = fat32_open,
    .close   = fat32_close,
    .read    = fat32_read,
    .write   = fat32_write,
    .flush   = fat32_flush,
    .finddir = fat32_finddir,
    .readdir = fat32_readdir,
    .mkdir   = fat32_mkdir,
    .create  = fat32_create,
    .unlink  = fat32_unlink,
    .rmdir   = fat32_rmdir,
    .stat    = fat32_stat, 
    .chmod   = fat32_chmod,
    .rename  = fat32_rename
    
};

//-----------------------------------------------------------------------------
// IMPLEMENTAÇÃO DO CALLBACK DE MONTAGEM PRINCIPAL (Otimizado para Partições)
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// CALLBACK DE MONTAGEM INTERNO (Otimizado para Partições MBR/GPT)
//-----------------------------------------------------------------------------
static vfs_node_t* fat32_mount_callback(block_device_t* dev, const char* mount_point) {
    if (!dev || !dev->read_blocks) return NULL;

    // Aloca um buffer temporário com base no tamanho do setor do seu block_device_t
    uint8_t* sector_buf = (uint8_t*)kmalloc(dev->sector_size);
    if (!sector_buf) return NULL;

    // LBA 0 aqui representa o início da partição virtual mapeada pelo partitions.c
    if (dev->read_blocks(dev, 0, 1, sector_buf) != 0) {
        kfree(sector_buf);
        return NULL;
    }

    fat32_bpb_t* bpb = (fat32_bpb_t*)sector_buf;

    // Valida as assinaturas clássicas do sistema FAT32
    if (bpb->boot_signature != 0x29 && bpb->boot_signature != 0x28) {
        kprintf("[FAT32] Erro: Assinatura FAT32 inválida (0x%X) em '%s'.\n", bpb->boot_signature, dev->name);
        kfree(sector_buf);
        return NULL;
    }

    // Aloca os metadados privados de controlo do volume
    fat32_volume_t* vol = (fat32_volume_t*)kmalloc(sizeof(fat32_volume_t));
    if (!vol) {
        kfree(sector_buf);
        return NULL;
    }

    vol->dev                 = dev;
    vol->bytes_per_sector    = bpb->bytes_per_sector;
    vol->sectors_per_cluster = bpb->sectors_per_cluster;
    vol->bytes_per_cluster   = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    vol->root_cluster        = bpb->root_cluster;
    
    // Fórmulas Oficiais da Microsoft para Mapeamento Linear de Setores FAT
    vol->first_fat_sector    = bpb->reserved_sector_count;
    vol->first_data_sector   = bpb->reserved_sector_count + (bpb->table_count * bpb->sectors_per_fat_32);

    // Cria a representação do nó raiz lógico do dispositivo no VFS
    vfs_node_t* root_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!root_node) {
        kfree(vol);
        kfree(sector_buf);
        return NULL;
    }

    memset(root_node, 0, sizeof(vfs_node_t));
    strcpy(root_node->name, mount_point);
    root_node->flags        = VFS_DIRECTORY;
    root_node->size         = 0;
    root_node->inode        = bpb->root_cluster; // Inode aponta para o cluster raiz
    root_node->ops          = &g_fat32_ops;      // Vincula a tabela de operações do FAT32
    root_node->private_data = vol;               // Guarda o contexto do disco
    root_node->fs           = NULL;              // Gerido dinamicamente pelo vfs_mount
    root_node->ptr_mount    = NULL;

    kprintf("[FAT32] Partição '%s' montada com sucesso em '%s'.\n", dev->name, mount_point);
    kprintf("[FAT32] Geometria: %d Bytes/Setor, %d Setores/Cluster, Setor Base de Dados: %llu\n",
            vol->bytes_per_sector, vol->sectors_per_cluster, vol->first_data_sector);

    kfree(sector_buf);
    return root_node;
}

//-----------------------------------------------------------------------------
// CALLBACK DE DESMONTAGEM (UNMOUNT)
//-----------------------------------------------------------------------------
static int fat32_unmount_callback(vfs_node_t* root_node) {
    if (!root_node) return -1;

    // 1. Extrai o contexto privado do volume acoplado ao nó raiz
    fat32_volume_t* vol = (fat32_volume_t*)root_node->private_data;
    if (vol) {
        // Nota: Se implementou caches de blocos/setores ou buffers "dirty" na RAM,
        // este é o momento exato para efetuar o flush (sincronização) para o disco rígido.
        kprintf("[FAT32] Sincronizando e libertando metadados do volume: %s\n", vol->dev->name);
        
        kfree(vol); // Liberta a estrutura interna fat32_volume_t
    }

    // 2. Liberta o próprio nó de ancoragem que foi alocado pelo VFS durante o boot
    kfree(root_node);

    kprintf("[FAT32] Volume desmontado com sucesso.\n");
    return 0; // Sucesso
}

/* Descritor global do Driver para o VFS */
static vfs_filesystem_t g_fat32_fs_driver = {
    .name    = "fat32",
    .mount   = fat32_mount_callback,
    .unmount = fat32_unmount_callback
};

/**
 * Inicialização e Registo Público do Driver FAT32
 */
void fat32_init(void) {
    vfs_register_filesystem(&g_fat32_fs_driver);
}