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
	 * 1. Informações do Boot
	 * 2. GDT
	 * 3. IDT
	 * 4. Paging / Virtual Memory
	 * 5. Gerenciador de memória
	 * 6. Kernel Heap
	 * 7. ACPI
	 * 8. Drivers
	 * 9. VFS
	 * 10. Scheduler
	 * 11. IPC
	 * 12. Modules
	 *
	 */

    setup_paging(boot_info);

    video_init(&boot_info->Graphics);

    // Teste
	for (int i = 0; i < 400; i++)
	{
		put_pixel(i, i, 0xFF0000);
	}

	for (;;)
	{
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
	}
}