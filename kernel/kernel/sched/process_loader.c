/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: process_loader.c
 *    Description: Carregador síncrono de binários ELF para a Pool de Memória.
 *                 Abstrai a leitura do disco e despacha para o Escalonador
 *                 suportando a passagem dinâmica de argc e argv.
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 13/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process_loader.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

/**
 * Lê um binário executável do disco para um buffer alinhado via pool_alloc,
 * aceita a matriz de argumentos do utilizador e despacha a sua instanciação.
 * 
 * @param path     Caminho absoluto do executável (ex: "/System/shell.elf").
 * @param argc     Contagem total de argumentos na linha de comandos.
 * @param argv     Array de strings contendo os argumentos literais.
 * @param cpu_id   ID do núcleo que agendará a thread principal.
 * @return Ponteiro para a estrutura do processo criado (PCB) ou NULL.
 */
process_t* elf_load_and_create_process(const char* path, int argc, char** argv, uint32_t cpu_id) {
    if (!path) return NULL;

    kprintf("[ELF] A carregar binário do disco: '%s'...\n", path);

    // 1. Abre o ficheiro através do VFS
    vfs_node_t* file = vfs_open(path, VFS_MODE_READ);
    if (!file) {
        kprintf("[ELF] Erro: Ficheiro '%s' não encontrado no VFS.\n", path);
        return NULL;
    }

    uint64_t binary_size = file->size;
    if (binary_size == 0) {
        kprintf("[ELF] Erro: Ficheiro binário está vazio (0 bytes).\n");
        vfs_close(file);
        return NULL;
    }

    // 2. ALINHAMENTO DMA OBRIGATÓRIO: Aloca o buffer da Pool arredondado para páginas (4KB)
    uint64_t alloc_size = (binary_size + 0xFFFUL) & ~0xFFFUL;
    void* binary_buffer = pool_alloc(alloc_size);
    if (!binary_buffer) {
        kprintf("[ELF] Erro: Falha ao alocar buffer de %llu bytes na Pool DMA.\n", alloc_size);
        vfs_close(file);
        return NULL;
    }

    // Limpa o buffer por segurança académica
    memset(binary_buffer, 0, alloc_size);

    // 3. Lê o binário completo do disco rígido para a RAM
    int bytes_lidos = vfs_read(file, 0, (uint32_t)binary_size, binary_buffer);
    vfs_close(file); // Fecha o ficheiro imediatamente após a leitura

    if (bytes_lidos != (int)binary_size) {
        kprintf("[ELF] Erro crítico: Falha de leitura síncrona no hardware.\n");
        pool_free(binary_buffer, alloc_size);
        return NULL;
    }

    kprintf("[ELF] Transferência concluída (%d bytes). Instanciando processo com %d argumento(s)...\n", 
            bytes_lidos, argc);

    // 4. CORREÇÃO CRÍTICA: Encaminha os argumentos recebidos para a criação da Stack
    process_t* proc = process_create(binary_buffer, binary_size, argc, argv, cpu_id);

    // 5. Liberta o buffer temporário da Pool após a criação do processo (Evita Memory Leak)
    pool_free(binary_buffer, alloc_size);

    return proc;
}