/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: clone.h
 *    Description: Definições e protótipos para o subsistema de clonagem
 *                 genérica de fluxos de execução (Processos e Threads).
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _CLONE_H_
#define _CLONE_H_

#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

/* 
 * Flags de controlo de partilha de recursos para a sys_clone()
 * Mapeadas de acordo com as especificações clássicas do Unix/Linux.
 */
#define CLONE_VM    0x00000100  /* Partilha o espaço de endereçamento virtual (CR3) */
#define CLONE_FILES 0x00000400  /* Partilha a tabela de descritores de arquivos abertos */

/**
 * clone - Cria um novo fluxo de execução concorrente (Thread ou Processo).
 * @frame: Ponteiro para o Stack Frame contendo os registadores salvos da CPU.
 *         O campo frame->rsi contém as flags binárias de configuração.
 * 
 * Detalhes Técnicos:
 * - Se a flag CLONE_VM estiver ativa, instancia um contexto partilhado (Thread).
 * - Se CLONE_VM estiver ausente, aloca um novo PCB, duplicando o espaço virtual 
 *   PML4 via vmm_clone_address_space.
 * - Copia a tabela de descritores caso a flag CLONE_FILES seja especificada.
 * 
 * Retorno:
 *   - No processo/thread Pai: Retorna o PID do novo processo ou o TID da thread.
 *   - No fluxo Filho: Retorna 0 (Configurado diretamente no frame->rax).
 *   - Em caso de falha: Retorna -1.
 */
pid_t clone(stack_frame_t* frame);

#endif /* _CLONE_H_ */
