/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kmod.h
 *    Description: Estruturas de controlo para o Gestor de Módulos (LKM).
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

#ifndef _KMOD_H_
#define _KMOD_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>
#include <kernel/lib/elf.h>

#define MODULE_NAME_MAX 64
/*
 * Assinaturas de funções obrigatórias para os módulos dinâmicos
 */
typedef int (*mod_init_t)(void);
typedef void (*mod_exit_t)(void);

/*
 * Estrutura de controlo de um Módulo carregado em memória (Ring 0)
 */
typedef struct module {
    char name[MODULE_NAME_MAX];     /* Nome identificador do módulo */
    void *module_core;              /* Endereço base do bloco alocado na RAM do Kernel */
    size_t core_size;               /* Tamanho total ocupado na memória */
    
    mod_init_t init;                /* Ponteiro para a rotina de inicialização (module_init) */
    mod_exit_t exit;                /* Ponteiro para a rotina de encerramento (module_exit) */
    
    void **section_allocs;          /* Array de ponteiros alocados para as secções deste módulo */
    size_t alloc_count;             /* Quantidade de blocos alocados */
    
    uint32_t state;                 /* Estado operacional do módulo (0=carregado, 1=ativo) */
    struct module *next;            /* Ponteiro para o próximo nó da lista ligada */
} module_t;

/*
 * Estrutura da Tabela de Símbolos exportados pelo Kernel principal
 */
typedef struct kernel_symbol {
    const char *name;               /* Nome da função ou variável global do kernel */
    uintptr_t address;              /* Endereço virtual correspondente na memória */
} kernel_symbol_t;

/*
 * Protótipos de funções globais para a gestão de módulos
 */
int kmod_init(void);
int kmod_load(const uint8_t *elf_buffer, size_t size);
int kmod_unload(const char *name);
uintptr_t kmod_find_symbol(const char *name);
void kmod_register_tracked_module(module_t *new_mod);
void kmod_print_all(void);
int kmod_load_by_name(const char *path);

#endif /* _KMOD_H_ */