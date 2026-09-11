/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: block.h
 *    Description: Abstração de Dispositivos de Armazenamento em Bloco.
 *                 Define a interface unificada de E/S, tabelas de registo 
 *                 de dispositivos e estruturas para operações síncronas/assíncronas.
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

#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <kernel/lib/stdint.h>

#define BLOCK_MAX_DEVICES 16
#define BLOCK_NAME_MAX    32

/**
 * Estrutura abstrata de um Dispositivo de Blocos (Block Device)
 * Atua como a interface intermédia entre os sistemas de ficheiros e os drivers.
 */
typedef struct block_device {
    char name[BLOCK_NAME_MAX];    /* Nome amigável do dispositivo (ex: "ahci0", "ramdisk") */
    uint32_t id;                 /* Identificador único gerido pelo subsistema de blocos */
    uint64_t total_sectors;      /* Capacidade total do dispositivo em setores */
    uint32_t sector_size;        /* Tamanho físico/lógico do setor (tipicamente 512 ou 4096) */
    
    /**
     * Ponteiros de Função (Interfaces de Hardware implementadas pelo Driver)
     * NOTA: Aceitam buffers virtuais (void*) para simplificar as chamadas no VFS.
     */
    int (*read_blocks)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
    int (*write_blocks)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
    int (*ioctl)(struct block_device* dev, uint32_t cmd, unsigned long arg);

    void* private_data;          /* Contexto privado do driver (ex: apontador para ahci_device_t) */
} block_device_t;

/* --- Interfaces Públicas de Gestão de Dispositivos --- */

/**
 * Inicializa o subsistema de controlo e tabelas de dispositivos de bloco.
 */
void block_subsystem_init(void);

/**
 * Regista um dispositivo de blocos ativo no catálogo global do Kernel.
 * Retorna o ID atribuído ao dispositivo em caso de sucesso, ou um número negativo em erro.
 */
int register_block_device(block_device_t* dev);

/**
 * Remove o registo de um dispositivo do catálogo global (ex: hot-unplug ou descarregamento de LKM).
 */
int unregister_block_device(uint32_t id);

/**
 * Procura um dispositivo de blocos registado pelo seu ID único.
 */
block_device_t* block_get_device(uint32_t id);

/**
 * Procura um dispositivo de blocos registado pelo seu nome literal.
 */
block_device_t* block_get_device_by_name(const char* name);

/**
 * Varre o catálogo global e imprime no terminal a lista de todos os
 * dispositivos de bloco ativos, os seus IDs e as respetivas capacidades.
 */
void block_list_devices(void);

#endif /* _BLOCK_H_ */
