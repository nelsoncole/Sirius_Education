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
#include <kernel/kernel.h>
#include <kernel/lib/stddef.h>

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
#define VFS_MODE_TRUNC  0x08

/* Diretrizes nativas para o vfs_seek (Padrão POSIX) */
#define VFS_SEEK_SET  0
#define VFS_SEEK_CUR  1
#define VFS_SEEK_END  2

struct vfs_node;
struct vfs_stat;
struct vfs_filesystem;


/**
 * Tabela de Operações do VFS (Polimorfismo em C)
 * Cada sistema de ficheiros (FAT32, NTFS, DevFS) implementará estas funções.
 */
typedef struct vfs_operations {
    int (*open)(struct vfs_node* node, uint32_t flags);
    int (*close)(struct vfs_node* node);
    
    // Leitura e escrita baseadas em offset de bytes
    int (*read)(struct vfs_node* node, uint64_t offset, uint32_t size, void* buffer);
    int (*write)(struct vfs_node* node, uint64_t offset, uint32_t size, void* buffer);
    
    // Força a sincronização de caches da RAM com o HDD/SSD
    int (*flush)(struct vfs_node* node);
    
    // Operações específicas para diretórios e ciclo de vida
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    int (*readdir)(struct vfs_node* node, uint32_t index, struct vfs_node* out_node);
    
    int (*mkdir)(struct vfs_node* node, const char* name, uint16_t permissions);
    int (*create)(struct vfs_node* node, const char* name, uint16_t permissions);
    
    // Remoção
    int (*unlink)(struct vfs_node* node, const char* name);
    int (*rmdir)(struct vfs_node* node, const char* name);
    
    // Preenche uma estrutura 'stat' com datas e atributos reais do disco
    int (*stat)(struct vfs_node* node, struct vfs_stat* buf);
    
    // Altera permissões/atributos (ex: ativar/desativar Read-Only no FAT32, NTFS, DevFS)
    int (*chmod)(struct vfs_node* node, uint16_t mode);
    
    // Move ou renomeia um arquivo/pasta de forma nativa no sistema de arquivos
    int (*rename)(struct vfs_node* node, const char* old_name, const char* new_name);
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

/* Estrutura de controlo de sessão de ficheiro para o processo */
typedef struct vfs_file {
    vfs_node_t* node;           // Ponteiro para o nó do VFS correspondente
    uint64_t    offset;         // Posição atual de leitura/escrita em bytes
    uint32_t    flags;          // O_RDONLY, O_WRONLY, O_RDWR
    uint32_t    ref_count;      // Contador de referências para partilha entre processos
} vfs_file_t;

typedef struct vfs_stat {
    uint32_t st_ino;       /* Número do Inode (No FAT32, mapeamos para o Cluster Inicial) */
    uint64_t st_size;      /* Tamanho real do ficheiro em Bytes */
    uint32_t st_mode;      /* Tipo e permissões lógicas (Diretório ou Ficheiro) */
    uint32_t st_uid;       /* ID do utilizador (Donos - Fixo como 0 no FAT32) */
    uint32_t st_gid;       /* ID do grupo (Fixo como 0 no FAT32) */
    uint16_t st_attr;      /* Atributos nativos brutos do hardware FAT32 (Hidden, System, etc) */
} vfs_stat_t;


/**
 * Estrutura de Definição de um Sistema de Ficheiros (FS Driver Descriptor)
 */
typedef struct vfs_filesystem {
    const char* name; /* Nome do driver. Ex: "fat32" */
    
    // Callback de Montagem: Lê o hardware e aloca a raiz do disco
    struct vfs_node* (*mount)(block_device_t* dev, const char* mount_path);
    
    // Callback de Desmontagem: Liberta as estruturas do volume e fecha o hardware
    int (*unmount)(struct vfs_node* root_node);

} vfs_filesystem_t;

/* Estrutura interna para mapeamento de montagens dinâmicas (Sem atropelar a raiz) */
typedef struct vfs_mount {
    vfs_node_t*       mountpoint;  // O nó da pasta nativa na RAM (ex: /mnt/usb)
    vfs_node_t*       fs_root;     // O nó de raiz real devolvido pelo driver (FAT32, ext2)
    struct vfs_mount* next;
} vfs_mount_t;


/* --- Interfaces Públicas do Núcleo do VFS --- */
/* 
 * VARIÁVEL GLOBAL DE INICIALIZAÇÃO EXPORTADA:
 * Permite que qualquer módulo consulte o nome da partição ativa de boot.
 */
extern char g_boot_partition_name[32];

/**
 * @brief Devolve o ponteiro para o nó raiz primitivo do Sistema de Ficheiros Virtual.
 * @return Ponteiro para vfs_node_t que representa a raiz '/'.
 */
vfs_node_t* vfs_get_root(void);
vfs_node_t* vfs_resolve_mountpoint(vfs_node_t* node);
vfs_node_t* vfs_path_to_node(const char* path);

/*Torna a tabela do ramfs.c visível para o vfs.c */
extern vfs_operations_t g_ramfs_ops;
/**
 * Inicializa la árvore virtual do VFS e monta a estrutura '/' RAM elementar.
 */
void vfs_init(void);

/**
 * Regista um driver de sistema de ficheiros (ex: chamado dentro de fat_init()).
 */
int vfs_register_filesystem(vfs_filesystem_t* fs);
/**
 * Varre iterativamente todo o catálogo de armazenamento global, localiza todas as
 * unidades de disco físicas brutas registadas e dispara o scanner síncrono MBR/GPT
 * para mapear dinamicamente todas as partições existentes na memória RAM.
 * 
 * @return 0 em caso de sucesso (pelo menos um disco processado), ou -1 se nenhum for encontrado.
 */
int vfs_init_partitions(void);
/**
 * Monta um dispositivo de bloco num caminho virtual usando um sistema de ficheiros específico.
 * Ex: vfs_mount("ahci0.1", "/", "fat32");
 */
int vfs_mount(const char* device_name, const char* mount_path, const char* fs_type);
int vfs_umount(const char* mount_path);

/* Operações Genéricas de E/S expostas para as Syscalls do Kernel */
vfs_node_t* vfs_open(const char* path, uint32_t flags);
int vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
int vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
void vfs_close(vfs_node_t* node);
vfs_node_t* vfs_finddir(vfs_node_t* parent, const char* name);
int vfs_readdir(vfs_node_t* target, uint32_t index, vfs_node_t* out_node);
int vfs_mkdir(vfs_node_t* parent, const char* name, uint16_t permissions);
int vfs_create(vfs_node_t* parent, const char* name, uint16_t permissions);
int vfs_flush(vfs_node_t* node);
int vfs_stat(vfs_node_t* node, vfs_stat_t* buf);
int vfs_chmod(vfs_node_t* node, uint16_t mode);
int vfs_unlink(vfs_node_t* parent, const char* name);
int vfs_rmdir(vfs_node_t* parent, const char* name);
int vfs_rename(vfs_node_t* parent, const char* old_name, const char* new_name);
uint64_t vfs_seek(vfs_file_t* file, int64_t offset, int whence);

struct process;
int k_dup2(struct process* proc, int oldfd, int newfd);

void vfs_print_tree(const char* start_path);


#endif /* _VFS_H_ */