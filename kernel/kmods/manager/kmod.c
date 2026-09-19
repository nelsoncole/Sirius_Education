/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kmod.c
 *    Description: Gestor central do ciclo de vida dos módulos do Kernel (LKM).
 *                 Controla o registo, inicialização e descarregamento de 
 *                 drivers e extensões dinâmicas em Ring 0.
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

#include <kernel/kmods/kmod.h>
#include <kernel/klib.h>

/* Cabeça da lista ligada de módulos carregados no Kernel */
static module_t *modules_head = NULL;

/*
 * Inicializa o subsistema de módulos dinâmicos do Kernel.
 */
int kmod_init(void) 
{
    modules_head = NULL;
    kprintf("[kmod]: Subsistema de modulos dinamicos inicializado.\n");
    return 0;
}

/*
 * Regista um módulo na lista global de monitorização do Kernel.
 * Esta função liga o novo módulo ao topo da estrutura da lista ligada.
 */
void kmod_register_tracked_module(module_t *new_mod) 
{
    if (!new_mod) 
    {
        return;
    }

    /* Insere o novo módulo no topo da lista ligada global */
    new_mod->next = modules_head;
    modules_head = new_mod;

    kprintf("[kmod]: Modulo '%s' registado no gestor central.\n", new_mod->name);
}

/*
 * Descarrega um módulo da memória do Kernel pelo nome.
 * Invoca a rotina de saída do módulo e liberta os recursos alocados.
 */
int kmod_unload(const char *name) 
{
    if (!name) 
    {
        return -1;
    }

    module_t *current = modules_head;
    module_t *previous = NULL;

    /* Procura o módulo na lista ligada global */
    while (current != NULL) 
    {
        if (strcmp(current->name, name) == 0) 
        {
            break;
        }
        previous = current;
        current = current->next;
    }

    /* Módulo não foi encontrado */
    if (!current) 
    {
        kprintf("[kmod]: Erro: Modulo '%s' nao encontrado para descarregar.\n", name);
        return -2;
    }

    kprintf("[kmod]: A descarregar o modulo '%s'...\n", current->name);

    /* 1. Executa a rotina de encerramento/limpeza do próprio módulo se existir */
    if (current->exit) 
    {
        (*current->exit)();
    }

    /* 2. Remove o nó da estrutura da lista ligada do Kernel */
    if (previous == NULL) 
    {
        /* O módulo a remover era o primeiro da lista */
        modules_head = current->next;
    } 
    else 
    {
        /* Remove o nó intermédio ou final */
        previous->next = current->next;
    }

    /* 
     * 3. Resolução da libertação de memória: Varre o array de ponteiros das
     * secções e devolve cada bloco alocado de volta para o Heap do Kernel.
     */
    if (current->section_allocs) 
    {
        for (size_t i = 0; i < current->alloc_count; i++) 
        {
            if (current->section_allocs[i] != NULL) 
            {
                kfree(current->section_allocs[i]);
            }
        }
        /* Liberta o array rastreador de ponteiros */
        kfree(current->section_allocs);
    }
    
    /* Liberta a própria estrutura de controlo do módulo */
    kfree(current);

    kprintf("[kmod]: Modulo '%s' descarregado com sucesso.\n", name);
    return 0;
}

/*
 * Imprime a listagem de todos os módulos atualmente carregados em memória.
 * Útil para comandos de diagnóstico da Shell (equivalente ao lsmod).
 */
void kmod_print_all(void)
{
    module_t *current = modules_head;

    kprintf("\n================ Sirius_Education LKM List ================\n");
    kprintf("%-20s %-12s %-10s %s\n", "Nome do Modulo", "Estado", "Seccoes", "Endereco Base");
    kprintf("-----------------------------------------------------------\n");

    if (current == NULL)
    {
        kprintf("(Nenhum modulo dinamico carregado no Kernel neste momento)\n");
    }

    while (current != NULL)
    {
        /* Mapeamento de texto simples do estado do módulo */
        const char *state_str = (current->state == 1) ? "ATIVO" : "CARREGADO";
        
        /* Obtém o endereço da primeira secção alocada como referência base visual */
        uintptr_t base_addr = (current->alloc_count > 0) ? (uintptr_t)current->section_allocs[0] : 0;

        kprintf("%-20s %-12s %-10d 0x%x\n", 
                current->name, 
                state_str, 
                current->alloc_count, 
                base_addr);

        current = current->next;
    }
    kprintf("===========================================================\n\n");
}