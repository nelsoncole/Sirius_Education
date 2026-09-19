/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kmod_loader.c
 *    Description: Carregador síncrono de módulos do Kernel a partir do VFS.
 *                 Abstrai a leitura do sistema de ficheiros e injeta o driver
 *                 diretamente no motor de gestão dinâmica (LKM).
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kmods/kmod.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

/**
 * Abre um ficheiro de módulo (.ko) do disco, transfere-o para um buffer
 * alinhado na Pool e invoca o parser ELF para o injetar em Ring 0.
 * 
 * @param path     Caminho absoluto do módulo (ex: "/lib/modules/sample_mod.ko").
 * @return 0 em caso de sucesso, ou código de erro negativo.
 */
int kmod_load_by_name(const char *path) 
{
    if (!path) 
    {
        return -1;
    }

    kprintf("[kmod]: A carregar driver do disco: '%s'...\n", path);

    /* 1. Abre o ficheiro do módulo através do VFS */
    vfs_node_t *file = vfs_open(path, VFS_MODE_READ);
    if (!file) 
    {
        kprintf("[kmod]: Erro: Modulo '%s' nao encontrado no VFS.\n", path);
        return -2;
    }

    uint64_t binary_size = file->size;
    if (binary_size == 0) 
    {
        kprintf("[kmod]: Erro: O arquivo do modulo esta vazio.\n");
        vfs_close(file);
        return -3;
    }

    /* 2. Aloca o buffer da Pool arredondado para o limite de páginas (4KB) */
    uint64_t alloc_size = (binary_size + 0xFFFUL) & ~0xFFFUL;
    void *binary_buffer = pool_alloc(alloc_size);
    if (!binary_buffer) 
    {
        kprintf("[kmod]: Erro: Falha ao alocar %llu bytes na Pool para leitura.\n", alloc_size);
        vfs_close(file);
        return -4;
    }

    /* 3. Lê o binário completo do módulo para a RAM */
    int bytes_lidos = vfs_read(file, 0, (uint32_t)binary_size, binary_buffer);
    vfs_close(file); /* Fecha o descritor imediatamente */

    if (bytes_lidos != (int)binary_size) 
    {
        kprintf("[kmod]: Erro critico: Falha de leitura sincrona do modulo.\n");
        pool_free(binary_buffer, alloc_size);
        return -5;
    }

    kprintf("[kmod]: Transferencia concluida (%d bytes). Injetando no Kernel...\n", bytes_lidos);

    /* 4. Encaminha o buffer carregado para o motor oficial de relocações ELF64 */
    int status = kmod_load((const uint8_t *)binary_buffer, binary_size);

    /* 5. Liberta o buffer temporário da Pool (Evita Memory Leak) */
    pool_free(binary_buffer, alloc_size);

    if (status != 0) 
    {
        kprintf("[kmod]: Erro ao processar estruturas ELF do modulo (Codigo: %d).\n", status);
        return status;
    }

    return 0; /* Sucesso absoluto */
}