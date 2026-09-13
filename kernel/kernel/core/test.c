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
#include <kernel/drivers/storage/partitions.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/kvmm.h>


/**
 * Varre um nó de diretório do VFS e imprime todos os ficheiros e pastas no terminal.
 */
static void vfs_list_directory(vfs_node_t* dir_node) {
    if (!dir_node) return;

    // Se for um ponto de montagem ativo, redireciona para a raiz real do disco
    if ((dir_node->flags & VFS_MOUNTPOINT) && dir_node->ptr_mount) {
        dir_node = dir_node->ptr_mount;
    }

    if (!dir_node->ops || !dir_node->ops->readdir) {
        kprintf("[VFS LIST] Erro: O sistema de ficheiros não suporta listagem.\n");
        return;
    }

    kprintf("\n========================================================================\n");
    kprintf("                       LISTAGEM DO DIRETÓRIO: %-25s\n", dir_node->name);
    kprintf("========================================================================\n");
    kprintf(" Nome                         | Tipo       | Tamanho (Bytes) | Inode    \n");
    kprintf("------------------------------+------------+-----------------+----------\n");

    uint32_t index = 0;
    vfs_node_t child_node;
    int count = 0;

    // Executa o loop incrementando o índice até o readdir do FAT32 retornar -1
    while (dir_node->ops->readdir(dir_node, index, &child_node) == 0) {
        const char* type_str = (child_node.flags & VFS_DIRECTORY) ? "DIRETÓRIO" : "FICHEIRO";
        
        kprintf(" %-28s | %-10s | %-15llu | %-8u\n", 
                child_node.name, 
                type_str, 
                child_node.size, 
                child_node.inode);

        index++;
        count++;
    }

    if (count == 0) {
        kprintf(" [Nenhum ficheiro ou pasta detetado neste volume]\n");
    }
    kprintf("========================================================================\n\n");
}

void test(void) 
{
    kprintf("[BOOT] A varrer dispositivos de armazenamento à procura de partições...\n");

    block_list_devices();
    
    // 7. MONTAGEM FINAL: Monta a partição virtual "ahci0.1" como a raiz real '/' do sistema
    kprintf("[BOOT] Montando a partição de boot como raiz do VFS...\n");
    int status = vfs_mount(g_boot_partition_name, "/", "fat32");

    if (status == 0) {
        kprintf("[BOOT] Sirius_Education arrancou com sucesso a partir do disco físico (ahci0.1).\n");
        
        // 8. LEITURA DO DIRETÓRIO RAIZ
        vfs_node_t* raiz_montada = vfs_open("/", 0);
        if (raiz_montada != NULL) {
            vfs_list_directory(raiz_montada);

            // -----------------------------------------------------------------
            // Bloco de Teste: Criar pasta 'test', criar 'text.txt', escrever e ler
            // -----------------------------------------------------------------
            kprintf("[TESTE] A criar a pasta 'test' na raiz...\n");
            if (raiz_montada->ops->mkdir) {
                raiz_montada->ops->mkdir(raiz_montada, "test", 0x01FF);
            }

            kprintf("[TESTE] A criar o ficheiro 'text.txt' na raiz...\n");
            if (raiz_montada->ops->create) {
                raiz_montada->ops->create(raiz_montada, "text.txt", 0x01FF);
            }

            // Abre o nó do ficheiro criado para escrita e leitura
            kprintf("[TESTE] A abrir 'text.txt' para validação de E/S...\n");
            vfs_node_t* file_txt = raiz_montada->ops->finddir(raiz_montada, "text.txt");
            if (file_txt != NULL) {
                const char* msg_escrever = "Sirius_Education";
                uint32_t msg_len = strlen(msg_escrever);

                if (file_txt->ops->write) {
                    kprintf("[TESTE] A escrever \"%s\" no ficheiro...\n", msg_escrever);
                    // Escreve a string no cluster inicial (offset 0)
                    int bytes_escritos = file_txt->ops->write(file_txt, 0, msg_len, (uint8_t*)msg_escrever);
                    
                    if (bytes_escritos > 0) {
                        // Atualiza dinamicamente o tamanho no nó em cache para permitir a leitura correta
                        file_txt->size = bytes_escritos; 
                        kprintf("[TESTE] Sucesso: %d bytes escritos.\n", bytes_escritos);
                    }
                }

                if (file_txt->ops->read) {
                    char buffer_leitura[64];
                    memset(buffer_leitura, 0, sizeof(buffer_leitura));

                    kprintf("[TESTE] A ler conteúdo de 'text.txt'...\n");
                    int bytes_lidos = file_txt->ops->read(file_txt, 0, msg_len, (uint8_t*)buffer_leitura);
                    
                    if (bytes_lidos > 0) {
                        buffer_leitura[bytes_lidos] = '\0'; // Garante terminação nula da string
                        kprintf("[TESTE] Conteúdo lido com sucesso: \"%s\"\n", buffer_leitura);
                    } else {
                        kprintf("[TESTE] Erro ou nenhum byte retornado na leitura.\n");
                    }
                }

                
                if (file_txt->ops->close) {
                    kprintf("[TESTE] A fechar o descritor e a sincronizar tabelas...\n");
                    file_txt->ops->close(file_txt);
                }
                                
                kfree(file_txt); // Liberta o nó do ficheiro de testes
            } else {
                kprintf("[TESTE] Erro: Não foi possível abrir o nó de 'text.txt' pós-criação.\n");
            }

            kprintf("[TESTE] Renomeando 'text.txt' para 'rename_text.txt' na raiz...\n");
            if (raiz_montada->ops->rename) {
                int res_rename = raiz_montada->ops->rename(raiz_montada, "text.txt", "rename_text.txt");
                if (res_rename == 0) {
                    kprintf("[TESTE] Sucesso: Ficheiro renomeado de forma nativa no disco!\n");
                } else {
                    kprintf("[TESTE] Erro ao renomear o ficheiro (Código: %d).\n", res_rename);
                }
            } else {
                kprintf("[TESTE] Erro: Operação 'rename' não suportada ou mapeada no driver.\n");
            }
            
            // -----------------------------------------------------------------

            kprintf("[TESTE] Nova listagem da raiz pós-modificações:\n");
            vfs_list_directory(raiz_montada);
            // -----------------------------------------------------------------
            
            // 9. ABERTURA E LISTAGEM DO DIRETÓRIO SYSTEM
            kprintf("[BOOT] A abrir subdiretório 'System'...\n");
            // Nota: Se a formatação visual do seu FAT32 mantém maiúsculas, use "System ou "system" conforme o caso.
            vfs_node_t* dir_system = raiz_montada->ops->finddir(raiz_montada, "System");
            
            if (dir_system != NULL) {
                vfs_list_directory(dir_system);
                
                // 10. ABERTURA DO FICHEIRO KERNEL.ELF E EXIBIÇÃO DE METADADOS
                kprintf("[BOOT] A ler metadados do ficheiro 'kernel.elf'...\n");
                vfs_node_t* file_kernel = dir_system->ops->finddir(dir_system, "kernel.elf");
                
                if (file_kernel != NULL) {
                    kprintf("\n========================================================================\n");
                    kprintf("                       METADADOS DE FICHEIRO ALOCADO                     \n");
                    kprintf("========================================================================\n");
                    kprintf(" Nome do Ficheiro : %s\n", file_kernel->name);
                    kprintf(" Tipo de Nó       : FICHEIRO REGULAR\n");
                    kprintf(" Tamanho Físico   : %llu Bytes\n", file_kernel->size);
                    kprintf(" Cluster Inicial  : %u (Mapeado como Inode)\n", file_kernel->inode);
                    kprintf(" Permissões Base  : 0x%X\n", file_kernel->permissions);
                    kprintf("========================================================================\n\n");
                    
                    // Liberta a estrutura alocada pelo finddir para o ficheiro
                    kfree(file_kernel);
                } else {
                    kprintf("[BOOT] Erro: Ficheiro 'kernel.elf' não encontrado dentro da pasta System.\n");
                }
                
                // Liberta a estrutura alocada pelo finddir para a pasta
                kfree(dir_system);
            } else {
                kprintf("[BOOT] Erro: Diretorio 'System' não encontrado na raiz fisica.\n");
            }
        } else {
            kprintf("[BOOT] Erro: Falha ao abrir o no '/' para listagem.\n");
        }
    } else {
        kprintf("[BOOT] Aviso: Falha ao montar ahci0.1. O sistema mantém-se em RAM.\n");
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}