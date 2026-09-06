/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: syscall.h
 *    Description: Cabeçalho da Camada de Abstração de Chamadas de Sistema (SCI).
 *                 Define a tabela de vetores (sys_call_table), os números
 *                 de identificação dos serviços e os protótipos de Ring 0.
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

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <kernel/lib/stdint.h>

/*
 * CONFIGURAÇÃO DOS NÚMEROS DE CHAMADA DE SISTEMA (SYSCALL NUMBERS)
 * ------------------------------------------------------------------------
 * Índices lógicos passados no registador RAX pelas aplicações em Ring 3.
 */
#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_BRK    2
#define SYS_EXIT   3

/* Número total de chamadas suportadas nesta fase inicial */
#define MAX_SYSCALLS 4

/**
 * Inicializa e programa os registadores de hardware MSR (STAR, LSTAR, FMASK)
 * locais do núcleo atual para ativar o suporte à instrução 'syscall'.
 * Deve ser executada individualmente pelo BSP e por cada AP no arranque.
 */
void syscall_init(void);

/**
 * Manipulador mestre em C (SCI Dispatcher). Recebe o fluxo do Stub em 
 * Assembly, valida o índice contido em RAX e despacha para a função correta.
 * 
 * @param syscall_num O ID do serviço (vindo de RAX mapeado para RDI).
 * @param arg1 Primeiro argumento da chamada (vindo de RDI mapeado para RSI).
 * @param arg2 Segundo argumento da chamada (vindo de RSI mapeado para RDX).
 * @param arg3 Terceiro argumento da chamada (vindo de RDX mapeado para RCX).
 * @return O valor de retorno da operação que será devolvido à aplicação em RAX.
 */
uint64_t syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

/*
 * ============================================================================
 * PROTÓTIPOS DOS SERVIÇOS NATIVOS INTERNOS DO KERNEL (HANDLERS)
 * ============================================================================
 */

/**
 * Lê dados de um descritor de ficheiro ou entrada padrão (Stub).
 */
uint64_t sys_read(void);

/**
 * Escreve uma cadeia de caracteres na consola do sistema a partir de Ring 3.
 * 
 * @param buffer Ponteiro virtual para a string em espaço de utilizador.
 * @param length Tamanho em bytes dos dados a imprimir.
 * @return O número de caracteres escritos com sucesso.
 */
uint64_t sys_write(const char* buffer, uint64_t length);

/**
 * Altera o limite de alocação dinâmica (Heap) do processo utilizador.
 * 
 * @param addr Novo endereço de fronteira virtual desejado para o Heap.
 * @return O endereço atualizado ou o limite anterior em caso de erro.
 */
uint64_t sys_brk(void* addr);

/**
 * Encerra a execução do processo atual e liberta os seus recursos.
 * 
 * @param code Código de status de finalização reportado pela aplicação.
 */
uint64_t sys_exit(int code);

#endif
