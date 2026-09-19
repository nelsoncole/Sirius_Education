/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: main.c
 *    Description: Módulo dinâmico de teste (LKM) para validação do 
 *                 subsistema de carregamento e gestão de símbolos do Kernel.
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

/*
 * Rotina de inicialização executada pelo loader.c no carregamento.
 */
int module_init(void) 
{
    kprintf("[sample_mod]: Inicializado com sucesso no Sirius_Education!\n");
    return 0;
}

/*
 * Rotina de encerramento executada pelo kmod.c no descarregamento.
 */
void module_exit(void) 
{
    kprintf("[sample_mod]: Removido do espaco de memoria do Kernel.\n");
}