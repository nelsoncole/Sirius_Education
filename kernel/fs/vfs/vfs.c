/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs.c
 *    Description: Implementação do Sistema de Ficheiros Virtual (VFS).
 *                 Gere a árvore de diretórios raiz, o catálogo de drivers
 *                 de ficheiros registados e a resolução de caminhos POSIX.
 * 
 *         Author: Nelson Cole
 *   Created Date: 11/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 16/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>
#include <kernel/drivers/storage/partitions.h>

#define FS_MAX_REG_DRIVERS 16

/* Tabelas Globais de Controlo do Subsistema */
static vfs_filesystem_t* g_registered_filesystems[FS_MAX_REG_DRIVERS];
static vfs_node_t*       g_vfs_root = NULL;
static vfs_mount_t*      g_mount_list = NULL;

/* Armazena o nome literal da partição física de boot (ex: "ahci0.1") */
char g_boot_partition_name[32] = {0};

/* Tranca rápida para tabelas em RAM */
static volatile uint64_t vfs_table_lock = 0;

void vfs_lock_tables(void) {
    while (__atomic_test_and_set(&vfs_table_lock, __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause" ::: "memory");
    }
}

void vfs_unlock_tables(void) {
    __atomic_clear(&vfs_table_lock, __ATOMIC_RELEASE);
}


/**
 * Intercepta o nó do VFS. Se este mascarar um ponto de montagem ativo,
 * desvia de forma atómica o fluxo para a raiz do dispositivo real.
 */
vfs_node_t* vfs_resolve_mountpoint(vfs_node_t* node) {
    if (!node || !(node->flags & VFS_MOUNTPOINT)) {
        return node;
    }

    vfs_lock_tables();
    vfs_mount_t* curr = g_mount_list;
    while (curr != NULL) {
        // CORREÇÃO DE SEGURANÇA: Compara por endereço OU por Inode idêntico
        // Isto garante que se o nó for uma cópia alocada pelo finddir, a ponte ainda é detetada!
        if (curr->mountpoint == node || 
           (curr->mountpoint->inode == node->inode && curr->mountpoint->ops == node->ops)) 
        {
            vfs_node_t* target = curr->fs_root;
            vfs_unlock_tables();
            return target; // Desvio síncrono bem-sucedido
        }
        curr = curr->next;
    }
    vfs_unlock_tables();
    return node;
}


/**
 * Percorre a árvore de diretórios resolvendo um caminho POSIX absoluto ou relativo.
 * Trata desvios de pontos de montagem dinâmicos e evita vazamentos de memória (leaks).
 * 
 * @param path O caminho literal (ex: "/mnt/usb/documento.txt").
 * @return O ponteiro para o vfs_node_t correspondente, ou NULL se não for encontrado.
 */
vfs_node_t* vfs_path_to_node(const char* path) {
    if (!path || path[0] == '\0') {
        return NULL;
    }

    vfs_node_t* current_node = NULL;
    
    if (path[0] == '/') {
        current_node = vfs_resolve_mountpoint(g_vfs_root);
    } else {
        current_node = vfs_resolve_mountpoint(g_vfs_root);
    }

    if (strcmp(path, "/") == 0) {
        return current_node;
    }

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* token = path_copy;
    if (*token == '/') token++; 

    while (token && *token != '\0') {
        char* next_slash = strchr(token, '/');
        if (next_slash != NULL) {
            *next_slash = '\0'; 
        }

        if (strcmp(token, ".") == 0) {
            if (next_slash != NULL) {
                token = next_slash + 1;
                continue;
            } else {
                break;
            }
        }
        
        current_node = vfs_resolve_mountpoint(current_node);

        // Verifica suporte a finddir
        if (!current_node->ops || !current_node->ops->finddir) {
            // REMOVIDO: kfree(current_node) daqui destruía nós estruturais ativos
            return NULL;
        }

        // Executa a busca pelo filho
        vfs_node_t* next_node = current_node->ops->finddir(current_node, token);
        
        if (!next_node) {
            // REMOVIDO: kfree(current_node) daqui destruía o pai quando a busca falhava
            return NULL;
        }

        /* 
         * REMOVIDO DAQUI: Libertação condicional do pai (kfree(current_node)).
         * Como os nós são persistentes na árvore do VFS e geridos pelos drivers,
         * libertá-los intermédio a intermédio corrompia a estrutura permanentemente.
         */

        current_node = next_node;

        if (next_slash != NULL) {
            token = next_slash + 1;
        } else {
            break;
        }
    }

    current_node = vfs_resolve_mountpoint(current_node);
    return current_node;
}

/**
 * vfs_get_parent_and_child - Isola o caminho, resolve e retorna o nó parente.
 * @path:       O caminho completo vindo de Ring 3 (ex: "/mnt/hd0/nova_pasta").
 * @out_child:  Ponteiro de memória para gravar apenas o nome do filho (ex: "nova_pasta").
 *              (Pode ser NULL se o utilizador apenas quiser o nó parente).
 * @return:     Ponteiro para o vfs_node_t do parente, ou NULL em caso de falha.
 */
vfs_node_t* vfs_get_parent_and_child(const char* path, char* out_child) {
    if (!path || path[0] == '\0') return NULL;

    size_t len = strlen(path);
    char parent_path[256];
    memset(parent_path, 0, sizeof(parent_path));

    // ============================================================================
    // REGRA DO NELSON: Se termina com '/', o parente é o próprio caminho completo
    // e o out_child[0] fica estritamente com '\0'.
    // ============================================================================
    if (len > 1 && path[len - 1] == '/') {
        // Copia o caminho inteiro preservando a barra final (ex: "/mnt/hd0/nelson/")
        strncpy(parent_path, path, sizeof(parent_path) - 1);
        parent_path[sizeof(parent_path) - 1] = '\0';

        // O filho fica garantidamente vazio
        if (out_child) {
            out_child[0] = '\0';
        }

        // Devolve o nó resolvido da própria pasta de destino
        return vfs_path_to_node(parent_path);
    }

    // ============================================================================
    // CASO PADRÃO: Caminho regular terminando em ficheiro (sem barra no fim)
    // ============================================================================
    int last_slash = -1;
    for (int i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash == -1) {
        // Se não possui barra, o parente é a raiz padrão "/"
        strcpy(parent_path, "/");
        if (out_child) strcpy(out_child, path);
    } 
    else {
        // Isola a string do caminho do diretório parente
        memcpy(parent_path, path, last_slash);
        parent_path[last_slash] = '\0';
        
        // Se a barra estava no índice 0, o parente é a raiz absoluta "/"
        if (last_slash == 0) {
            strcpy(parent_path, "/");
        }
        
        // Isola o nome bruto do filho
        if (out_child) {
            strcpy(out_child, &path[last_slash + 1]);
        }
    }

    return vfs_path_to_node(parent_path);
}


//-----------------------------------------------------------------------------
// API DE ACESSO AO CORE DO VFS
//-----------------------------------------------------------------------------

/**
 * @brief Devolve o ponteiro para o nó raiz primitivo do Sistema de Ficheiros Virtual.
 * @return Ponteiro para vfs_node_t que representa a raiz '/'.
 */
vfs_node_t* vfs_get_root(void) {
    return g_vfs_root;
}


//-----------------------------------------------------------------------------
// INICIALIZAÇÃO DO SUBSISTEMA VFS
//-----------------------------------------------------------------------------
void vfs_init(void) {
    vfs_unlock_tables();
    g_mount_list = NULL;

    // 1. Limpa o catálogo de drivers de sistemas de ficheiros
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        g_registered_filesystems[i] = NULL;
    }

    // 2. Cria o nó raiz primitivo '/' em memória RAM (Rootfs Elementar)
    g_vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (g_vfs_root) {
        memset(g_vfs_root, 0, sizeof(vfs_node_t));
        strncpy(g_vfs_root->name, "/", sizeof(g_vfs_root->name) - 1);
        g_vfs_root->flags = VFS_DIRECTORY;
        g_vfs_root->size = 0;
        g_vfs_root->inode = 1;
        g_vfs_root->private_data = NULL; // Começa sem filhos na RAM

        /* Diz à raiz que ela fala o dialeto do RamFS */
        g_vfs_root->ops          = &g_ramfs_ops; 
        g_vfs_root->fs           = NULL;
        g_vfs_root->ptr_mount    = NULL;
        
        kprintf("[VFS] Virtual File System Root '/' inicializado com sucesso em RAM.\n");
    } else {
        kprintf("[VFS] CRÍTICO: Falha catastrofica ao alocar o no raiz do sistema.\n");
    }
}

/**
 * Localiza e identifica o dispositivo de boot fidedigno confrontando os metadados.
 */
const char* vfs_identify_boot_partition(DEVICE_PATH_INFO* boot_device) {
    if (!boot_device || boot_device->PartitionSize == 0) {
        kprintf("[VFS BOOT] Erro: Dados da particao ativa de boot ausentes ou invalidos.\n");
        return NULL;
    }

    uint32_t target_part = (boot_device->PartitionNumber == 0) ? 1 : boot_device->PartitionNumber;

    kprintf("[VFS BOOT] A identificar particao via UEFI: ID .%u, LBA Inicial %lu\n", 
            target_part, boot_device->PartitionStart);

    // Protegemos a leitura concorrente do catálogo de blocos
    vfs_lock_tables();
    for (int i = 0; i < 32; i++) {
        block_device_t* dev = block_get_device_by_index(i); 
        if (dev == NULL || dev->private_data == NULL) {
            continue; 
        }

        partition_ctx_t* ctx = (partition_ctx_t*)dev->private_data;

        if (ctx->partition_num == target_part && ctx->start_lba == boot_device->PartitionStart) {
            if (ctx->mbr_type == boot_device->MBRType && ctx->signature_type == boot_device->SignatureType) {
                if (memcmp(ctx->signature, boot_device->Signature, 16) == 0) {
                    const char* table_type_str = (ctx->mbr_type == 2) ? "GPT" : "MBR";
                    
                    strncpy(g_boot_partition_name, dev->name, sizeof(g_boot_partition_name) - 1);
                    g_boot_partition_name[sizeof(g_boot_partition_name) - 1] = '\0';

                    kprintf("[VFS BOOT] Match perfeito! Dispositivo resolvido de forma fidedigna: '%s' [%s]\n", 
                            g_boot_partition_name, table_type_str);
                    
                    vfs_unlock_tables();
                    return g_boot_partition_name; 
                }
            }
        }
    }

    g_boot_partition_name[0] = '\0';
    kprintf("[VFS BOOT] Aviso: Nenhuma particao em cache coincide com os dados binarios do UEFI.\n");
    
    vfs_unlock_tables();
    return g_boot_partition_name;
}

/**
 * Varre o catálogo global e dispara o scanner síncrono MBR/GPT.
 */
int vfs_init_partitions(void) {
    kprintf("[BOOT] A varrer o catalogo de armazenamento global a procura de particoes...\n");

    int devices_scanned = 0;

    for (int i = 0; i < 32; i++) {
        block_device_t* dev = block_get_device_by_index(i);
        if (dev == NULL) continue;

        if (dev->is_raw == true) {
            kprintf("[BOOT] Unidade fisica '%s' detetada. A iniciar varredura MBR/GPT...\n", dev->name);
            partition_scan_device(dev);
            devices_scanned++;
        }
    }

    if (devices_scanned == 0) {
        kprintf("[BOOT] Erro Crítico: Nenhuma unidade física de armazenamento encontrada no catálogo.\n");
        return -1;
    }

    const char* boot_devi_name = vfs_identify_boot_partition(&g_boot_info->BootDevice);
    kprintf("[BOOT] Particao de inicializacao do sistema identificada: '%s'\n", boot_devi_name);
    kprintf("[BOOT] Varredura de partições concluida. %d unidade(s) fisica(s) mapeada(s).\n", devices_scanned);
    return 0;
}

//-----------------------------------------------------------------------------
// REGISTO DE DRIVERS DE SISTEMAS DE FICHEIROS
//-----------------------------------------------------------------------------
int vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!fs || !fs->name) return -1;

    vfs_lock_tables();
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        if (g_registered_filesystems[i] == NULL) {
            g_registered_filesystems[i] = fs;
            kprintf("[VFS] Driver de Sistema de Ficheiros '%s' registado.\n", fs->name);
            vfs_unlock_tables();
            return 0;
        }
    }
    vfs_unlock_tables();
    return -2;
}

//-----------------------------------------------------------------------------
// OPERAÇÃO DE MONTAGEM (MOUNT) DINÂMICA
//-----------------------------------------------------------------------------
int vfs_mount(const char* device_name, const char* mount_path, const char* fs_type) {
    if (!mount_path || !fs_type) return -1;

    // 1. Procura o dispositivo de bloco (Leitura segura)
    block_device_t* dev = NULL;
    if (device_name != NULL) {
        dev = block_get_device_by_name(device_name);
        if (!dev) {
            kprintf("[VFS MOUNT] Erro: Dispositivo '%s' nao encontrado.\n", device_name);
            return -2;
        }
    }

    // 2. Procura o driver correspondente no catálogo (Protegido por spinlock)
    vfs_filesystem_t* fs_driver = NULL;
    vfs_lock_tables();
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        if (g_registered_filesystems[i] != NULL && strcmp(g_registered_filesystems[i]->name, fs_type) == 0) {
            fs_driver = g_registered_filesystems[i];
            break;
        }
    }
    vfs_unlock_tables();

    if (!fs_driver) {
        kprintf("[VFS MOUNT] Erro: Driver de FS '%s' nao esta registado.\n", fs_type);
        return -3;
    }

    // 3. RESOLUÇÃO DINÂMICA DO NÓ ALVO (Suporta pontos de montagem secundários)
    vfs_node_t* target_mp_node = vfs_path_to_node(mount_path);
    if (!target_mp_node) {
        kprintf("[VFS MOUNT] Erro: O caminho do ponto de montagem '%s' nao existe.\n", mount_path);
        return -5;
    }

    // Garante que o alvo é de facto um diretório e não um ficheiro comum
    if ((target_mp_node->flags & VFS_DIRECTORY) == 0) {
        kprintf("[VFS MOUNT] Erro: O caminho '%s' nao e um diretorio valido.\n", mount_path);
        // Se o nó foi alocado dinamicamente pelo path_to_node, evita leak libertando-o
        if (target_mp_node != g_vfs_root && target_mp_node != vfs_resolve_mountpoint(g_vfs_root)) {
            kfree(target_mp_node);
        }
        return -8;
    }

    /* 
     * Chamamos o mount do driver FORA de spinlocks.
     * O driver vai ler setores físicos do disco. Se usássemos spinlock aqui, 
     * o sistema sofreria um congelamento severo por contenção de CPU.
     */
    vfs_node_t* fs_root_node = fs_driver->mount(dev, mount_path);
    if (!fs_root_node) {
        kprintf("[VFS MOUNT] Erro: O driver '%s' falhou ao ler o dispositivo.\n", fs_type);
        if (target_mp_node != g_vfs_root && target_mp_node != vfs_resolve_mountpoint(g_vfs_root)) {
            kfree(target_mp_node);
        }
        return -4;
    }

    // 4. Criação da ponte de montagem (Alocação local isolada)
    vfs_mount_t* new_mount = (vfs_mount_t*)kmalloc(sizeof(vfs_mount_t));
    if (!new_mount) {
        if (target_mp_node != g_vfs_root && target_mp_node != vfs_resolve_mountpoint(g_vfs_root)) {
            kfree(target_mp_node);
        }
        return -7;
    }
    new_mount->mountpoint = target_mp_node;
    new_mount->fs_root = fs_root_node;

    // 5. ZONA CRÍTICA: Atualiza as tabelas globais do sistema de forma atómica
    vfs_lock_tables();
    
    // Atualiza o nó alvo para indicar que ele agora mascara um sistema de ficheiros
    target_mp_node->ptr_mount = fs_root_node;
    target_mp_node->flags |= VFS_MOUNTPOINT;

    // Insere o registo na lista encadeada global de montagens
    new_mount->next = g_mount_list;
    g_mount_list = new_mount;

    vfs_unlock_tables();

    kprintf("[VFS MOUNT] Sucesso: '%s' montado em '%s' usando '%s'.\n", 
            device_name ? device_name : "Virtual", mount_path, fs_type);
    
    return 0;
}

//-----------------------------------------------------------------------------
// OPERAÇÃO DE DESMONTAGEM (UMOUNT) DINÂMICA
//-----------------------------------------------------------------------------
int vfs_umount(const char* mount_path) {
    if (!mount_path) return -1;

    // 1. Resolve o caminho para obter o nó físico onde o sistema está montado
    vfs_node_t* mp_node = vfs_path_to_node(mount_path);
    if (!mp_node) {
        kprintf("[VFS UMOUNT] Erro: O caminho '%s' nao existe.\n", mount_path);
        return -4;
    }

    // 2. ZONA CRÍTICA: Localizar e remover a ponte de montagem da lista global
    vfs_lock_tables();
    
    // Verifica se o nó realmente mascara um ponto de montagem ativo
    if (!(mp_node->flags & VFS_MOUNTPOINT) || !mp_node->ptr_mount) {
        vfs_unlock_tables();
        kprintf("[VFS UMOUNT] Erro: Nao existe nenhum volume montado em '%s'.\n", mount_path);
        // Liberta o nó obtido pelo path_to_node se for dinâmico para evitar leak
        if (mp_node != g_vfs_root) kfree(mp_node);
        return -2;
    }

    vfs_mount_t* prev = NULL;
    vfs_mount_t* curr = g_mount_list;
    vfs_mount_t* target_mnt = NULL;

    // Varre a lista encadeada global para extrair a ponte de forma segura
    while (curr != NULL) {
        if (curr->mountpoint == mp_node) {
            target_mnt = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                g_mount_list = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    if (!target_mnt) {
        vfs_unlock_tables();
        kprintf("[VFS UMOUNT] Erro Interno: Inconsistencia na árvore de montagens para '%s'.\n", mount_path);
        if (mp_node != g_vfs_root) kfree(mp_node);
        return -5;
    }

    // Restaura o nó nativo original na RAM: desvincula o disco físico e remove a flag
    vfs_node_t* fs_root_to_free = mp_node->ptr_mount;
    mp_node->ptr_mount = NULL;
    mp_node->flags &= ~VFS_MOUNTPOINT;

    vfs_unlock_tables();

    kprintf("[VFS UMOUNT] Desmontando volume em '%s'...\n", mount_path);

    // 3. PERSISTÊNCIA (FORA DE SPINLOCKS): Sincroniza buffers pendentes com o disco
    if (fs_root_to_free->ops && fs_root_to_free->ops->flush) {
        fs_root_to_free->ops->flush(fs_root_to_free);
    }

    // 4. Invoca o polimorfismo do Driver (ex: FAT32) para libertar recursos privados de hardware
    if (fs_root_to_free->fs && fs_root_to_free->fs->unmount) {
        fs_root_to_free->fs->unmount(fs_root_to_free);
    } else {
        // Fallback genérico se o driver não expuser um método unmount explícito
        kfree(fs_root_to_free);
    }

    // 5. Limpeza de memória das pontes de controlo em RAM
    kfree(target_mnt);
    
    // Liberta o nó de pesquisa do mountpoint se ele não for a raiz global permanente
    if (mp_node != g_vfs_root) {
        kfree(mp_node);
    }

    kprintf("[VFS UMOUNT] Sucesso: Volume removido com seguranca de '%s'.\n", mount_path);
    return 0;
}

//-----------------------------------------------------------------------------
// OPERAÇÕES GENÉRICAS DE E/S (INVOCAÇÃO POLIMÓRFICA DIRETA)
//-----------------------------------------------------------------------------
/* 
 * Nota de Arquitetura: Como validado anteriormente, ler os ponteiros ->ops->funcao
 * NÃO requer spinlock de tabelas globais do VFS. O desvio de Mountpoint é resolvido.
 */

//-----------------------------------------------------------------------------
// OPERAÇÃO CENTRAL DE ABERTURA DE FICHEIROS (VFS OPEN)
//-----------------------------------------------------------------------------

/**
 * vfs_open - Abre ou cria de forma dinâmica um nó no VFS do Kernel.
 *            Garante isolamento de drivers polimórficos e proteção contra memory leaks.
 */
#define O_CREAT     0x0200
vfs_node_t* vfs_open(const char* path, uint32_t flags) {
    if (!path || path[0] == '\0') return NULL;

    // 1. Tenta resolver o caminho por inteiro através do motor de Name Lookup central
    vfs_node_t* target_node = vfs_path_to_node(path);
    
    // 2. Se o ficheiro não existe, mas a flag O_CREAT está ativa, aciona a fábrica
    if (!target_node) {
        if (flags & O_CREAT) {
            char file_name[64];
            
            // Puxa de forma limpa o nó parente real e o nome isolado do filho
            vfs_node_t* parent_node = vfs_get_parent_and_child(path, file_name);
            
            if (parent_node && parent_node->ops && parent_node->ops->create) {
                // Executa a criação física no RamFS ou FAT32 (Modo padrão de escrita: 0644)
                int res = parent_node->ops->create(parent_node, file_name, 0644);
                
                if (res == 0) {
                    // Re-avalia o caminho para capturar o nó virtual recém-nascido na RAM
                    target_node = vfs_path_to_node(path);
                    if (target_node) goto process_driver_open;
                }
            }
        }
        return NULL; // Ficheiro ou diretório intermédio real não encontrado
    }

process_driver_open:

    /* 3. BLINDAGEM DEFENSIVA EXTREMA: Garantia contra pânicos de ponteiro nulo */
    if (!target_node) {
        return NULL;
    }

    // 4. Dispara a inicialização polimórfica específica do driver (ex: clusters, caches)
    if (target_node->ops && target_node->ops->open) {
        int res_open = target_node->ops->open(target_node, flags);
        
        if (res_open != 0) {
            kprintf("[VFS OPEN] Erro: O driver falhou ao abrir o ficheiro '%s' (Codigo: %d).\n", path, res_open);
            
            // CORREÇÃO: Removeu-se o bloco destrutivo de kfree().
            // Se o driver falhou a abertura, o nó NÃO deve ser apagado da RAM,
            // pois ele pertence à estrutura viva do VFS/Mount. Apenas abortamos a abertura.
            return NULL; 
        }
    }

    // Devolve o nó pronto e testado pelo driver para o sys_open encapsular no FD
    return target_node; 
}


int vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;
    node = vfs_resolve_mountpoint(node);

    if (node->ops && node->ops->read) {
        return node->ops->read(node, offset, size, buffer);
    }
    return -2;
}

int vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;
    node = vfs_resolve_mountpoint(node);

    if (node->flags & VFS_DIRECTORY) {
        return -21; // Is a directory: Erro padrão POSIX! (-EISDIR)
    }

    if (node->ops && node->ops->write) {
        return node->ops->write(node, offset, size, buffer);
    }
    return -2;
}

/**
 * vfs_close - Fecha o acesso a um nó do VFS sem desalocar a sua estrutura viva.
 * @node: Ponteiro para o nó de Ring 0 que foi manipulado pelo descritor.
 */
void vfs_close(vfs_node_t* node) {
    if (!node) return;

    // 1. Resolve o nó real do hardware caso o ficheiro seja um ponto de montagem
    vfs_node_t* real_node = vfs_resolve_mountpoint(node);
    
    // 2. Notifica o driver de armazenamento (ex: FAT32, RamFS) para encerrar as rotinas da sessão
    if (real_node->ops && real_node->ops->close) {
        real_node->ops->close(real_node); // Executa o fecho a nível de hardware/FS
    }
    
    // 3. GESTÃO DE MEMÓRIA:
    // REMOVIDO EM DEFINITIVO: Qualquer chamada a kfree(node) ou kfree(real_node).
    // Como os teus nós são persistentes na RAM e geridos de forma estática 
    // pelas montagens (mounts), fechar um ficheiro ou diretório NÃO pode apagar 
    // a sua estrutura da topologia viva do VFS.
}

int vfs_flush(vfs_node_t* node) {
    if (!node) return -1;
    node = vfs_resolve_mountpoint(node);
    if (node->ops && node->ops->flush) {
        return node->ops->flush(node);
    }
    return -2;
}

//-----------------------------------------------------------------------------
// OPERAÇÕES POLIMÓRFICAS DE DIRETÓRIOS E CICLO DE VIDA (VFS API)
//-----------------------------------------------------------------------------

/**
 * Procura por um elemento (ficheiro ou pasta) pelo nome dentro de um diretório.
 * 
 * @param parent O nó do diretório pai.
 * @param name   O nome literal do elemento a procurar.
 * @return Um novo vfs_node_t preenchido pelo driver, ou NULL se não existir.
 */
vfs_node_t* vfs_finddir(vfs_node_t* parent, const char* name) {
    if (!parent || !name || name[0] == '\0') return NULL;
    
    // Se o pai for um mountpoint, desvia para a raiz do sistema de ficheiros real
    parent = vfs_resolve_mountpoint(parent);

    if (parent->ops && parent->ops->finddir) {
        return parent->ops->finddir(parent, name);
    }
    return NULL;
}

/**
 * Enumera iterativamente os elementos de um diretório com base num índice.
 * Útil para implementar a syscall 'getdents' ou um comando 'ls'.
 * 
 * @param target   O nó do diretório a ser listado.
 * @param index    O índice do elemento (0, 1, 2...).
 * @param out_node Estrutura de saída a ser preenchida com os dados do elemento encontrado.
 * @return 0 em caso de sucesso, ou um código negativo em caso de erro/fim do diretório.
 */
int vfs_readdir(vfs_node_t* target, uint32_t index, vfs_node_t* out_node) {
    if (!target || !out_node) return -1;
    
    target = vfs_resolve_mountpoint(target);

    if (target->ops && target->ops->readdir) {
        return target->ops->readdir(target, index, out_node);
    }
    return -2; // Operação não suportada por este nó
}

/**
 * Cria um novo diretório (pasta) dentro do diretório pai especificado.
 * 
 * @param parent      O nó do diretório pai onde a nova pasta será inserida.
 * @param name        O nome da nova pasta.
 * @param permissions As permissões POSIX de criação (ex: 0777).
 * @return 0 em caso de sucesso, ou um código negativo em caso de erro.
 */
int vfs_mkdir(vfs_node_t* parent, const char* name, uint16_t permissions) {
    if (!parent || !name || name[0] == '\0') return -1;
    
    parent = vfs_resolve_mountpoint(parent);

    if (parent->ops && parent->ops->mkdir) {
        return parent->ops->mkdir(parent, name, permissions);
    }
    return -2;
}

/**
 * Cria um novo ficheiro regular vazio dentro do diretório pai especificado.
 * 
 * @param parent      O nó do diretório pai onde o ficheiro será inserido.
 * @param name        O nome do novo ficheiro (ex: "texto.txt").
 * @param permissions As permissões POSIX de criação.
 * @return 0 em caso de sucesso, ou um código negativo em caso de erro.
 */
int vfs_create(vfs_node_t* parent, const char* name, uint16_t permissions) {
    if (!parent || !name || name[0] == '\0') return -1;
    
    parent = vfs_resolve_mountpoint(parent);

    if (parent->ops && parent->ops->create) {
        return parent->ops->create(parent, name, permissions);
    }
    return -2;
}

int vfs_stat(vfs_node_t* node, vfs_stat_t* buf) {
    if (!node || !buf) return -1;
    node = vfs_resolve_mountpoint(node);
    if (node->ops && node->ops->stat) {
        return node->ops->stat(node, buf);
    }
    return -2;
}

int vfs_chmod(vfs_node_t* node, uint16_t mode) {
    if (!node) return -1;
    node = vfs_resolve_mountpoint(node);
    if (node->ops && node->ops->chmod) {
        return node->ops->chmod(node, mode);
    }
    return -2;
}

int vfs_unlink(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    parent = vfs_resolve_mountpoint(parent);
    if (parent->ops && parent->ops->unlink) {
        return parent->ops->unlink(parent, name);
    }
    return -2;
}

int vfs_rmdir(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    parent = vfs_resolve_mountpoint(parent);
    if (parent->ops && parent->ops->rmdir) {
        return parent->ops->rmdir(parent, name);
    }
    return -2;
}

int vfs_rename(vfs_node_t* old_dir, const char* old_name, vfs_node_t* new_dir, const char* new_name) {
    if (!old_dir || !new_dir || !old_name || !new_name) return -1;

    // Resolve os pontos de montagem de ambos os diretórios para garantir que operamos no hardware correto
    old_dir = vfs_resolve_mountpoint(old_dir);
    new_dir = vfs_resolve_mountpoint(new_dir);

    // Invoca o driver correspondente (FAT32, RamFS, etc.) injetando os 4 argumentos
    if (old_dir->ops && old_dir->ops->rename) {
        return old_dir->ops->rename(old_dir, old_name, new_dir, new_name);
    }

    return -2; // Operação não suportada pelo driver do sistema de ficheiros
}

//-----------------------------------------------------------------------------
// OPERAÇÃO DE POSICIONAMENTO DE PONTEIRO (SEEK)
//-----------------------------------------------------------------------------
/**
 * Altera a posição do ponteiro de leitura/escrita (offset) de um ficheiro aberto.
 * 
 * @param file   Ponteiro para a estrutura de sessão do ficheiro aberto.
 * @param offset Deslocamento em bytes (pode ser negativo para caminhos relativos).
 * @param whence Diretriz de posicionamento:
 *               VFS_SEEK_SET (0): Define o offset absoluto a partir do início.
 *               VFS_SEEK_CUR (1): Ajusta o offset relativo à posição atual.
 *               VFS_SEEK_END (2): Ajusta o offset relativo ao fim do ficheiro.
 * 
 * @return O novo offset absoluto em bytes a partir do início, ou -1 em caso de erro.
 */
uint64_t vfs_seek(vfs_file_t* file, int64_t offset, int whence) {
    if (!file || !file->node) {
        return (uint64_t)-1;
    }

    vfs_node_t* node = file->node;

    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    uint64_t new_offset = file->offset;

    switch (whence) {
        case VFS_SEEK_SET:
            if (offset < 0) { return (uint64_t)-1; }
            new_offset = (uint64_t)offset;
            break;

        case VFS_SEEK_CUR:
            if (offset < 0 && ((uint64_t)(-offset) > file->offset)) {
                return (uint64_t)-1;
            }
            new_offset = file->offset + offset;
            break;

        case VFS_SEEK_END:
            if (offset < 0 && ((uint64_t)(-offset) > node->size)) {
                return (uint64_t)-1;
            }
            new_offset = node->size + offset;
            break;

        default:
            kprintf("[VFS SEEK] Erro: Diretriz 'whence' (%d) inválida.\n", whence);
            return (uint64_t)-1;
    }

    // Proteção de sanidade (opcional): Impede que o ponteiro vá além dos limites físicos 
    // se o ficheiro for aberto estritamente em modo de leitura. Em modo de escrita, 
    // caminhos POSIX permitem fazer seek além do fim para criar "gaps" vazios (sparse files).
    if (!(file->flags & VFS_MODE_WRITE) && (new_offset > node->size)) {
        new_offset = node->size;
    }

    // Aplica a nova posição lógica
    file->offset = new_offset;

    return file->offset;
}


//-----------------------------------------------------------------------------
// FERRAMENTA DE DIAGNÓSTICO: IMPRESSÃO DA ÁRVORE DO VFS
//-----------------------------------------------------------------------------
/**
 * Função interna recursiva para varrer e listar a árvore de diretórios de forma segura.
 */
static void vfs_print_tree_recursive(vfs_node_t* dir, int depth) {
    if (!dir || !(dir->flags & VFS_DIRECTORY)) return;

    // Tabela de operações da RAM para checagem de persistência
    extern vfs_operations_t g_ramfs_ops;

    vfs_node_t child_info;
    uint32_t index = 0;

    while (vfs_readdir(dir, index, &child_info) == 0) {
        
        if (strcmp(child_info.name, ".") == 0 || strcmp(child_info.name, "..") == 0) {
            index++;
            continue; 
        }

        for (int i = 0; i < depth; i++) {
            kprintf("  │");
        }

        if (child_info.flags & VFS_DIRECTORY) {
            kprintf("  ├── [-] %s/\n", child_info.name);

            vfs_node_t* real_child = vfs_finddir(dir, child_info.name);
            if (real_child) {
                vfs_print_tree_recursive(real_child, depth + 1);
                
                // CORREÇÃO CIRÚRGICA: real_child em vez de real_node
                // Só desaloca o nó se ele NÃO pertencer à estrutura persistente da RAMFS!
                if (real_child != g_vfs_root && real_child->ops != &g_ramfs_ops) {
                    kfree(real_child);
                }
            }
        } else {
            kprintf("  ├── [*] %s\n", child_info.name);
        }

        index++;
    }
}

/**
 * API Pública para disparar a impressão da árvore do VFS (Segura contra Use-After-Free).
 */
void vfs_print_tree(const char* start_path) {
    kprintf("[VFS TREE] A mapear árvore de diretórios a partir de '%s':\n", start_path);
    
    extern vfs_operations_t g_ramfs_ops;
    vfs_node_t* start_node = vfs_path_to_node(start_path);
    if (!start_node) {
        kprintf("[VFS TREE] Erro: Caminho de partida invalido.\n");
        return;
    }

    kprintf("[-] %s\n", start_path);
    vfs_print_tree_recursive(start_node, 0);
    kprintf("[VFS TREE] Fim da listagem.\n");

    // Fecha o nó inicial apenas se ele foi gerado dinamicamente fora da RAM estável
    if (start_node != g_vfs_root && start_node->ops != &g_ramfs_ops) {
        kfree(start_node);
    }
}

