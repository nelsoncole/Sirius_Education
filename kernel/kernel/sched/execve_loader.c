/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: execve_loader.c
 *    Description: Carregador síncrono de binários ELF para a Pool de Memória.
 *                 Abstrai a leitura do disco e sobrepõe o contexto do processo
 *                 atual (execve), suportando a passagem dinâmica de argc e argv.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 27/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process_loader.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/dev/vfs_pty.h>
#include <kernel/klib.h>
#include <kernel/kvmm.h>

/**
 * Lê um binário executável do disco para um buffer alinhado via pool_alloc,
 * aceita a matriz de argumentos e substitui a imagem do processo atual (execve).
 * 
 * @param path     Caminho absoluto do executável (ex: "/System/shell.elf").
 * @param argc     Contagem total de argumentos na linha de comandos.
 * @param argv     Array de strings contendo os argumentos literais.
 * @param proc     Ponteiro para a estrutura do processo atual (PCB).
 * @return Retorna 0 em caso de sucesso. Em caso de erro pós-flushing, aborta o processo.
 */
int elf_load_and_execve(const char* path, int argc, char** argv, process_t* proc) {
    if (!path || !proc || !argv) return -1;

    kprintf("[Process] Execve: A carregar binário do disco: '%s'...\n", path);

    interrupts_enable();

    // 1. Abre o ficheiro através do VFS
    vfs_node_t* file = vfs_open(path, VFS_MODE_READ);
    if (!file) {
        kprintf("[Process] Erro: Ficheiro '%s' não encontrado no VFS.\n", path);
        interrupts_disable();
        return -1;
    }

    uint64_t binary_size = file->size;
    if (binary_size == 0) {
        kprintf("[Process] Erro: Ficheiro binário está vazio (0 bytes).\n");
        vfs_close(file);
        interrupts_disable();
        return -1;
    }

    // 2. Aloca o buffer da Pool arredondado para páginas (4KB)
    uint64_t alloc_size = (binary_size + 0xFFFUL) & ~0xFFFUL;
    void* binary_buffer = pool_alloc(alloc_size);
    if (!binary_buffer) {
        kprintf("[Process] Erro: Falha ao alocar buffer de %llu bytes na Pool DMA.\n", alloc_size);
        vfs_close(file);
        interrupts_disable();
        return -1;
    }

    // 3. Lê o binário completo do disco rígido para a RAM
    int bytes_lidos = vfs_read(file, 0, (uint32_t)binary_size, binary_buffer);
    vfs_close(file); // Fecha o ficheiro imediatamente após a leitura

    if (bytes_lidos != (int)binary_size) {
        kprintf("[Process] Erro crítico: Falha de leitura síncrona no hardware.\n");
        pool_free(binary_buffer, alloc_size);
        interrupts_disable();
        return -1;
    }

    interrupts_disable();

    kprintf("[Process] Transferência concluída (%d bytes). Expurgando User Space antigo...\n", bytes_lidos);
    uint64_t cr3 = proc->cr3;
    proc->cr3 = vmm_create_address_space();
    if (proc->cr3 == 0)
    {
        proc->cr3 = cr3;
        kprintf("[Process] Erro: Falha critica ao criar espaco de memoria.\n");
        pool_free(binary_buffer, alloc_size);
        return -1;
    }

    // 5. MAPEAMENTO ELF MODULAR: Executa o parser estrutural no espaço limpo
    uintptr_t entry_point = elf_parse_and_map(proc, binary_buffer, binary_size, argc, argv);
    if (entry_point == 0)
    {
        kprintf("[Process] Erro Fatal: Falha ao mapear a estrutura do binário ELF no execve.\n");
        kprintf("[Process] Terminando de imediato o processo %d devido a estado inconsistente.\n", proc->pid);
        
        pool_free(binary_buffer, alloc_size);
        
        return -1; // Inalcançável, mas previne avisos do compilador
    }

    kprintf("[Process] Instanciando nova imagem com %d argumento(s). Entry Point: 0x%lx\n", argc, entry_point);
    
    // Limpeza terminal de resíduos alocados a meio do mapeamento
    vmm_switch_pml4(proc->cr3);
    pool_free(binary_buffer, alloc_size);

    process_flush_user_space(cr3);

    return 0;
}