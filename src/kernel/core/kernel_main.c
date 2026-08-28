/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kernel_main.c
 *    Description: Ponto de entrada (Main) do kernel independente de arquitetura.
 *                 Inicializa os subsistemas globais do sistema operativo.
 * 
 *         Author: Nelson Cole
 *   Created Date: 25/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 27/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel.h>
#include <kernel/drivers/video.h>

void kernel_main(BOOT_INFO *boot_info)
{
	/*
	 * O bootloader entregou as informações para o kernel.
	 */
	if (boot_info == 0)
	{
		/*
		 * Boot information inválida.
		 */
		for (;;)
		{
			__asm__ volatile("cli");
			__asm__ volatile("hlt");
		}
	}

	/*
	 * ============================================================
	 * Inicialização do Kernel
	 * ============================================================
	 *
	 * Ordem básica:
	 *
	 * 1. Paging
	 * 2. Console
	 * 3. Inicializar o PMM (Physical Memory Manager)
	 * 4. Inicializar o VMM (Virtual Memory Manager) / Kernel Heap
	 * 5. Alocar o CpuDataBlock do BSP (Core Principal)
	 * 6. Preenche a GDT e o TSS dentro do cpu_blocks[0]
	 * 7. Executa a instrução LGDT apontando para cpu_blocks[0]->gdtr
	 * 8. Configura o MSR GS_BASE do Core 0 para apontar para cpu_blocks[0]
	 * 9. Inicializar a IDT Global
	 * 10. ACPI
	 * 11. Drivers
	 * 12. VFS
	 * 13. Scheduler
	 * 14. IPC
	 * 15. Modules
	 *
	 */

    setup_paging(boot_info);

	// Inicializa o ecrã com as configurações do UEFI
    video_init(&boot_info->Graphics);

    // Limpa o ecrã para começar o desenho do zero
    fb_clear();

    // 3. Mensagem de Boas-Vindas Estruturada (Testa \n e \t)
    fb_print("========================================================================\n");
    fb_print("                     SIRIUS EDUCATION KERNEL x86_64                     \n");
    fb_print("========================================================================\n\n");
    
    fb_print("[OK] Video framebuffer inicializado com sucesso.\n");
    fb_print("[OK] Fonte bitmap VGA 8x16 carregada.\n");
    fb_print("[OK] Ponto de entrada de baixo nivel (entry.asm) operacional.\n\n");

    fb_print("Configuracoes detetadas pelo Bootloader:\n");
    fb_print("----------------------------------------\n");
    fb_print("  * Resolucao da Tela:\t");
    // (Mais tarde usaremos kprintf aqui, por agora vamos simular com strings fixas)
    fb_print("Ativa via UEFI\n");
    fb_print("  * Arquitetura:\t\tx86_64 Long Mode\n");
    fb_print("  * Status do SMP:\t\tSuporte para ate 256 nucleos configurado\n\n");

    fb_print("========================================================================\n");
    fb_print("Inicializando subsistemas de memoria (PMM / VMM)...\n");
    fb_print("========================================================================\n");

    /* 
     * TESTE DE SCROLL REAL:
     * Vamos imprimir várias linhas consecutivas para estourar o limite 
     * vertical da resolução e forçar o ecrã a rolar para cima.
     */
    for (int i = 1; i <= 40; i++) {
        fb_print("A testar a estabilidade do sistema... Linha de log numero \n");
    }

    fb_print("\n[SUCESSO] Se consegue ler isto no fundo da tela, o Scroll funciona!\n");

	for (;;)
	{
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
	}
}