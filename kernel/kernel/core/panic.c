/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: panic.c
 *    Description: Subsistema de tratamento de falhas críticas do Kernel 
 *                 (Kernel Panic), forçando a paragem segura do processador.
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 30/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/lib/stdio.h>
#include <kernel/kernel/core/panic.h>

/*
 * INTERRUPÇÃO CONTROLADA DO SISTEMA (KERNEL PANIC)
 * ------------------------------------------------------------------------
 * Bloqueia a execução de todas as tarefas suspendendo as interrupções da CPU.
 * Exibe visualmente o erro no ecrã e coloca o processador x86_64 em HLT.
 * 
 * @param message String de texto descritiva detalhando a causa da falha.
 */
void kernel_panic(const char* message) {
    /*
     * 1. DESATIVAÇÃO DE HARDWARE
     * Desativa imediatamente o mascaramento de interrupções para impedir 
     * que os temporizadores (timers) ou dispositivos acordem a CPU.
     */
    __asm__ __volatile__("cli");

    /*
     * 2. REGISTO VISUAL DO ERRO
     * Emite um alerta visual claro e chamativo no ecrã do sistema antes do 
     * congelamento total do hardware.
     */
    kprintf("\n==================================================\n");
    kprintf(" !!! KERNEL PANIC !!!\n");
    kprintf("--------------------------------------------------\n");
    kprintf(" O Sirius_Education encontrou uma falha critica:\n\n");
    kprintf(" %s\n", message);
    kprintf("==================================================\n");
    kprintf(" O sistema foi suspenso para proteger os seus dados.");

    /*
     * 3. CICLO DE PARAGEM PERMANENTE
     * Coloca o núcleo do processador em paragem física. Se uma interrupção 
     * não mascarável (NMI) conseguir acordar a CPU, o loop rebloqueia-a.
     */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
