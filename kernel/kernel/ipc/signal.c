/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: signal.c
 *    Description: Subsistema de sinais assíncronos. Permite a comunicação e
 *                 interrupção de tarefas alterando o estado de execução de
 *                 processos remotos.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/ipc/ipc.h>
#include <kernel/klib.h>

/* Declarações externas de dependências do Escalonador Core */
extern process_t* get_process_by_pid(pid_t pid);
extern void process_terminate(process_t* proc);

/**
 * Envia um sinal de interrupção ou controlo para um processo.
 * 
 * @param target_pid ID do processo de destino.
 * @param sig        Número do sinal POSIX equivalente a injetar.
 * @return 0 em caso de sucesso, -1 se houver falhas ou PID inexistente.
 */

int sys_kill(pid_t target_pid, int sig) 
{
    /* Validação defensiva do limite de sinais mapeados */
    if (sig < 0 || sig >= MAX_SIGNALS) return -1;

    /* Tenta localizar o PCB do processo-alvo na lista ativa do escalonador */
    process_t* target = get_process_by_pid(target_pid);
    if (!target) return -1;

    /* Tratamento imperativo para terminação imediata e irrecuperável */
    if (sig == SIGKILL) 
    {
        process_terminate(target);
        return 0;
    }

    /* Sinalização binária por Bitmask: ativa o bit correspondente ao sinal */
    /* Exemplo: sinal 2 (SIGINT) -> ativa o bit 2 (0x4) no inteiro pendente */
    target->pending_signals |= (1 << sig);
    
    return 0;
}