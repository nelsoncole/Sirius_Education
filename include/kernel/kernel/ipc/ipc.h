/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ipc.h
 *    Description: Interface pública da API de Utilizador (UAPI) para IPC.
 *                 Define os códigos de sinais POSIX, flags de controlo de SHM
 *                 e os protótipos das chamadas de sistema expostas ao User Space.
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

#ifndef _IPC_H_
#define _IPC_H_

#include <kernel/lib/stdint.h>

/* ========================================================================
 * DEFINIÇÃO DE SINAIS PADRÃO (POSIX EQUIVALENTE)
 * ======================================================================== */
#define SIGHUP      1   // Terminação de controlo de terminal (Hangup)
#define SIGINT      2   // Interrupção via teclado (Ctrl+C)
#define SIGQUIT     3   // Abortar execução com Core Dump
#define SIGILL      4   // Instrução ilegal detetada pelo CPU
#define SIGTRAP     5   // Trap de depuração (Breakpoint)
#define SIGABRT     6   // Sinal de aborto gerado por falha interna
#define SIGFPE      8   // Exceção aritmética de vírgula flutuante / Divisão por zero
#define SIGKILL     9   // Terminação imediata, forçada e irrecuperável do processo
#define SIGUSR1     10  // Sinal definido pelo utilizador 1
#define SIGSEGV     11  // Falha de segmentação / Acesso inválido à memória
#define SIGUSR2     12  // Sinal definido pelo utilizador 2
#define SIGPIPE     13  // Escrita num Pipe partido (sem leitores)
#define SIGALRM     14  // Alarme de temporizador disparado
#define SIGTERM     15  // Solicitação de terminação amigável (Padrão do kill)

/* ========================================================================
 * FLAGS DE CONTROLO DE MEMÓRIA PARTILHADA (SHM)
 * ======================================================================== */
#define SHM_CREAT   0x0200  // Cria o segmento se ele não existir na tabela global
#define SHM_EXCL    0x0400  // Falha a criação se o segmento já existir
#define SHM_RDONLY  0x0800  // Mapeia a região estritamente com permissões de leitura

/**
 * Solicita ou cria um identificador de memória partilhada associado a uma chave.
 * 
 * @param key   Chave numérica global geradora do recurso IPC.
 * @param flags Máscara de bits com opções de criação (ex: SHM_CREAT).
 * @return ID válido do segmento no Kernel ou -1 em caso de falha.
 */
int sys_shm_get(int key, int flags);

/**
 * Acopla (atacha) a página física partilhada no espaço virtual do processo.
 * O Kernel escolhe automaticamente o endereço linear seguro no User Space
 * com base na macro USER_SHM_VIRTUAL_BASE definida na arquitetura.
 * 
 * @param shmid Identificador numérico do segmento obtido via shm_get.
 * @return Ponteiro virtual linear para o bloco de 4KB ou (void*)-1 se falhar.
 */
void* sys_shm_at(int shmid);

/**
 * Desvincula (detacha) a região de memória partilhada do espaço do processo.
 * A página física é mantida no Kernel até que todos os processos façam detach.
 * 
 * @param shmid Identificador numérico do segmento a desligar.
 * @return 0 em caso de sucesso, -1 se o ID for inválido.
 */
int sys_shm_dt(int shmid);

/**
 * Envia um sinal assíncrono para controlo de estado de um processo alvo.
 * 
 * @param pid ID do processo de destino.
 * @param sig Número do sinal POSIX (ex: SIGKILL, SIGTERM) a ser injetado.
 * @return 0 em caso de sucesso, ou -1 se o processo não for localizado.
 */
int sys_kill(int pid, int sig);

#endif