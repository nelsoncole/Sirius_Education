/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: process_ipc_clean.c
 *    Description: Rotinas de desalocação e limpeza de recursos IPC vinculados.
 *                 Garante a remoção segura de referências e previne memory leaks
 *                 aquando do encerramento (exit) de um processo.
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
#include <kernel/kernel/mm/pmm.h>
#include <kernel/klib.h>

/* Funções externas dos subsistema SHM global */
extern void detach_shm_segment(int shmid);

/**
 * Varre e limpa todas as conexões e tabelas de IPC vinculadas a um PCB morto.
 * 
 * @param proc Ponteiro para o Bloco de Controlo de Processo a ser expurgado.
 */
void process_ipc_cleanup(process_t* proc) 
{
    if (!proc) return;

    /* 
     * 1. LIMPEZA DA MEMÓRIA PARTILHADA 
     * Liberta as amarras do processo das páginas físicas partilhadas globals.
     */
    for (int i = 0; i < MAX_SHARED_REGIONS; i++) 
    {
        if (proc->shm_ids[i] != 0) 
        {
            /* Decrementa o contador de referências global. Se chegar a 0, o frame físico é liberto */
            detach_shm_segment(proc->shm_ids[i]);
            proc->shm_ids[i] = 0;
            proc->shm_virtual_addresses[i] = NULL;
        }
    }
}