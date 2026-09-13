/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs.h
 *    Description: Núcleo do Sistema de Ficheiros Virtual (Virtual File System Core).
 *                 Define as abstrações universais para nós (vfs_node_t), 
 *                 operações de ficheiros/diretórios e tabelas de montagem.
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

#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/lib/stdint.h>
#include <kernel/drivers/storage/block.h>

#define VFS_NAME_MAX 256

/* Tipos de Nós de Ficheiro (Node Flags) */
#define VFS_FILE        (1 << 0)   /* Ficheiro regular */
#define VFS_DIRECTORY   (1 << 1)   /* Diretório / Pasta */
#define VFS_CHAR_DEV    (1 << 2)   /* Dispositivo de Caracteres (ex: teclado, tty) */
#define VFS_BLOCK_DEV   (1 << 3)   /* Dispositivo de Blocos (ex: ahci_disk) */
#define VFS_PIPE        (1 << 4)   /* Pipe IPC */
#define VFS_MOUNTPOINT  (1 << 5)   /* Ponto de Montagem Ativo */

/* Modos de Abertura de Ficheiros */
#define VFS_MODE_READ   0x01
#define VFS_MODE_WRITE  0x02
#define VFS_MODE_CREATE 0x04

struct vfs_node;
struct vfs_filesystem;

/**
 * Tabela de Operações do VFS (Polimorfismo em C)
 * Cada sistema de ficheiros (FAT32, NTFS, DevFS) implementará estas funções.
 */
typedef struct vfs_operations {
    int (*open)(struct vfs_node* node, uint32_t flags);
    int (*close)(struct vfs_node* node);
    
    // Leitura e escrita baseadas em offset de bytes (Trabalho do VFS/Driver de FS)
    int (*read)(struct vfs_node* node, uint64_t offset, uint32_t size, void* buffer);
    int (*write)(struct vfs_node* node, uint64_t offset, uint32_t size, void* buffer);
    
    // Operações específicas para diretórios
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    int (*readdir)(struct vfs_node* node, uint32_t index, struct vfs_node* out_node);
    
    int (*mkdir)(struct vfs_node* node, const char* name, uint16_t permissions);
    int (*create)(struct vfs_node* node, const char* name, uint16_t permissions);
} vfs_operations_t;

/**
 * O Nó do Sistema de Ficheiros Virtual (VFS Node / Inode Genérico)
 * Representa qualquer objeto lógico dentro da árvore de ficheiros.
 */
typedef struct vfs_node {
    char name[VFS_NAME_MAX];        /* Nome do ficheiro ou pasta (ex: "kernel.elf") */
    uint32_t flags;                 /* Tipo do nó (VFS_FILE, VFS_DIRECTORY, etc.) */
    uint64_t size;                  /* Tamanho do ficheiro em bytes */
    uint32_t inode;                 /* Identificador numérico interno do sistema de ficheiros */
    uint32_t permissions;           /* Permissões POSIX de acesso */
    
    vfs_operations_t* ops;          /* Tabela de funções correspondente ao tipo de FS */
    struct vfs_filesystem* fs;      /* Ponteiro para o sistema de ficheiros ao qual pertence */
    void* private_data;             /* Dados privados do driver (ex: cluster inicial no FAT) */
    
    struct vfs_node* ptr_mount;     /* Se for um ponto de montagem, aponta para a raiz mapeada */
} vfs_node_t;

/**
 * Estrutura de Definição de um Sistema de Ficheiros (FS Driver Descriptor)
 */
typedef struct vfs_filesystem {
    const char* name;               /* Nome do driver (ex: "fat32", "ntfs", "devfs") */
    
    // Função chamada quando 'vfs_mount' liga um disco a este sistema de ficheiros
    vfs_node_t* (*mount)(block_device_t* dev, const char* mount_point);
    int (*unmount)(vfs_node_t* root_node);
} vfs_filesystem_t;

/* --- Interfaces Públicas do Núcleo do VFS --- */

/**
 * Inicializa la árvore virtual do VFS e monta a estrutura '/' RAM elementar.
 */
void vfs_init(void);

/**
 * Regista um driver de sistema de ficheiros (ex: chamado dentro de fat_init()).
 */
int vfs_register_filesystem(vfs_filesystem_t* fs);

/**
 * Monta um dispositivo de bloco num caminho virtual usando um sistema de ficheiros específico.
 * Ex: vfs_mount("ahci0.1", "/", "fat32");
 */
int vfs_mount(const char* device_name, const char* mount_path, const char* fs_type);

/**
 * Resolve caminhos absolutos e abre um descritor de nó virtual.
 */
vfs_node_t* vfs_open(const char* path, uint32_t flags);

/* Operações Genéricas de E/S expostas para as Syscalls do Kernel */
int vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
int vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
void vfs_close(vfs_node_t* node);

#endif /* _VFS_H_ */