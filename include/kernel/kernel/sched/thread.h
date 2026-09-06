/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: thread.h
 *    Description: Cabeçalho para gestão do ciclo de vida de Threads.
 *                 Define os protótipos de criação, alocação de pilhas e
 *                 configuração de contexto para tarefas do kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 05/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _THREAD_H_
#define _THREAD_H_

#include <kernel/lib/stdint.h>
#include <kernel/arch/x86_64/cpu/reg.h>
/* Pré-declaração para evitar loops com o process.h */
struct process;

/*
 * ----------------------------------------------------------------------------
 * Estados de Execução de uma Thread
 * ----------------------------------------------------------------------------
 */
typedef enum {
    THREAD_READY,       // Pronta para ser executada pelo CPU
    THREAD_RUNNING,     // Em execução ativa num núcleo
    THREAD_BLOCKED,     // Bloqueada a aguardar I/O, IPC ou Mutex
    THREAD_DEAD         // Terminada, aguarda limpeza pelo coletor de lixo
} thread_state_t;

/*
 * ----------------------------------------------------------------------------
 * TCB (Thread Control Block)
 * ----------------------------------------------------------------------------
 */
typedef struct thread {
    uint32_t tid;               // Identificador único da Thread (Thread ID)
    void* kernel_stack;         // Ponteiro para o topo do stack de kernel (salvaguarda de contexto)
    thread_state_t state;       // Estado atual de execução
    uint32_t cpu_id;            // ID do CPU associado (ou fixado por afinidade)

    struct process* owner; /* Processo ao qual esta thread pertence */
    
    struct thread* next;        // Ponteiro para a próxima thread na fila (Runqueue)
} thread_t;

/**
 * Estrutura que mapeia o layout exato dos dados que o teu handler
 * em Assembly (e o CPU) esperam encontrar no topo da pilha ao restaurar.
 */
typedef registers_t stack_frame_t;

/**
 * Cria e configura uma nova thread de Kernel, forjando o seu stack inicial.
 * @param entry_point Ponteiro para a função C que a thread vai executar.
 * @param cpu_id O núcleo de processamento onde a thread será inicialmente agendada.
 * @return Ponteiro para a estrutura thread_t criada, ou NULL em caso de falha.
 */
thread_t* thread_create(void (*entry_point)(void), uint32_t cpu_id);

/**
 * Cria e configura uma nova thread de Utilizador (Ring 3), com contexto e isolamento adequados.
 */
thread_t* user_thread_create(void (*entry_point)(void), void* user_stack_top, uint32_t cpu_id);

#endif /* THREAD_H */
