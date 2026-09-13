/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: partitions.h
 *    Description: Subsistema de Gestão de Partições (MBR e GPT).
 *                 Abstrai partições físicas em dispositivos de bloco virtuais.
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

#ifndef _PARTITIONS_H_
#define _PARTITIONS_H_

#include <kernel/drivers/storage/block.h>

/* Estrutura de uma entrada da Tabela de Partições MBR (16 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  drive_status;     /* 0x80 = Bootable, 0x00 = Inativo */
    uint8_t  chs_start[3];     /* Endereço CHS inicial (Antigo) */
    uint8_t  partition_type;   /* Tipo da partição (ex: 0x0C = FAT32 LBA, 0xEE = GPT Protective) */
    uint8_t  chs_end[3];       /* Endereço CHS final */
    uint32_t start_lba;        /* LBA absoluto do primeiro setor da partição */
    uint32_t total_sectors;    /* Quantidade total de setores na partição */
} mbr_entry_t;

/* Cabeçalho de Partição GPT (LBA 1) */
typedef struct __attribute__((packed)) {
    char     signature[8];     /* Deve ser "EFI PART" */
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;      /* LBA que contém este cabeçalho (normalmente 1) */
    uint64_t backup_lba;       /* LBA do cabeçalho GPT secundário (fim do disco) */
    uint64_t first_usable_lba; /* Primeiro LBA utilizável para partições */
    uint64_t last_usable_lba;  /* Último LBA utilizável */
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba; /* LBA inicial da tabela de entradas de partição */
    uint32_t num_partition_entries;  /* Número de partições possíveis (tipicamente 128) */
    uint32_t size_partition_entry;   /* Tamanho de cada entrada (tipicamente 128) */
    uint32_t partition_array_crc32;
} gpt_header_t;

/* Entrada de Partição GPT Standard (128 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  partition_type_guid[16]; /* GUID do tipo (Zeros = Entrada Livre) */
    uint8_t  unique_partition_guid[16];
    uint64_t starting_lba;            /* Setor inicial real no disco */
    uint64_t ending_lba;              /* Setor final real no disco */
    uint64_t attributes;
    uint16_t partition_name[36];      /* Nome UTF-16LE */
} gpt_entry_t;

/* Contexto Privado anexado a cada partição virtual */
typedef struct {
    block_device_t* phys_dev;  /* Ponteiro para o disco rígido físico bruto (ex: sda) */
    uint64_t start_lba;        /* Offset inicial de setores no hardware */
    uint64_t total_sectors;    /* Capacidade lógica restrita da partição */
} partition_ctx_t;

/**
 * Faz o scan do dispositivo físico, identifica a tabela (MBR/GPT)
 * e regista dinamicamente as partições encontradas no catálogo global.
 */
void partition_scan_device(block_device_t* phys_dev);

#endif /* _PARTITIONS_H_ */