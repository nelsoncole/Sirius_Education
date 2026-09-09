/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: process.h
 *    Description: Cabeçalho do subsistema de processos (Process Management).
 *                 Define a estrutura do Bloco de Controlo do Processo (PCB),
 *                 estados de isolamento e protótipos para gestão de tarefas.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <kernel/klib.h>
#include "thread.h"
#include "scheduler.h"

#define MAX_SHARED_REGIONS 8
#define MAX_SIGNALS        32
#define MAX_SIGNALS            32
#define MAX_FILES_PER_PROCESS  32

// Estrutura abstrata do VFS (Virtual File System) definida em fs/vfs/
// struct file; 

typedef uint32_t pid_t;

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct process {
    pid_t pid;                      /* Identificador único do processo (PID) */
    pid_t ppid;                     /* ID do processo pai (Parent PID) */
    process_state_t state;          /* Estado de execução atual do processo */
    uint64_t cr3;                   /* Endereço físico do PML4 (Espaço de Memória) */
    
    /* Controlo e Limites do Espaço de Endereçamento do Aplicativo */
    uint64_t code_base;             /* Endereço virtual base do executável */
    uint64_t heap_start;            /* Endereço virtual base do Heap do utilizador */
    uint64_t heap_end;              /* Apontador para o fim atual do Heap dinâmico */
    uint64_t stack_limit;           /* Limite inferior da Pilha (para proteção) */
    uint64_t stack_top;             /* Topo absoluto atual da Pilha virtual */

    struct process* parent;         /* Ponteiro para o processo pai */
    thread_t* main_thread;          /* Ponteiro para a thread principal do processo */

    /*
     * TABELA DE FDs (Para Sockets, Pipes e Ficheiros) 
     * Ex: fd = 0 (stdin), fd = 1 (stdout), fd = 2 (stderr), fd = 3 (Socket/Ficheiro)
     */
    // struct file* file_descriptor_table[MAX_FILES_PER_PROCESS];

    /* IPC: MEMÓRIA PARTILHADA */
    void* shm_virtual_addresses[MAX_SHARED_REGIONS];
    int   shm_ids[MAX_SHARED_REGIONS];

    /* IPC: SINAIS ASSÍNCRONOS */
    uint32_t pending_signals;
    void* signal_handlers[MAX_SIGNALS];

} process_t;

/**
 * Cria um novo processo com o seu próprio espaço de endereçamento.
 * Mapeia o contexto inicial e vincula a sua thread principal ao agendador.
 * 
 * @param entry_point Ponteiro para a função de entrada do código.
 * @param cpu_id ID do núcleo onde a tarefa principal será inicialmente injetada.
 * @return Ponteiro para a estrutura PCB criada ou NULL em caso de falha.
 */
process_t* process_create(void* binary_buffer, unsigned long binary_size, uint32_t cpu_id);

/**
 * Destrói e liberta as estruturas e recursos associados a um processo.
 * 
 * @param proc Ponteiro para o Bloco de Controlo do Processo (PCB) a eliminar.
 */
void process_destroy(process_t* proc);

#endif /* PROCESS_H */