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
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel.h>
#include <kernel/drivers/video.h>
#include <kernel/lib/stdio.h>
#include <kernel/kernel/mm/pmm.h>

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

	/*
	 * 1. Paging
	 */
	setup_paging(boot_info);

	/*
	 * 2. Console (Vídeo / Framebuffer)
	 */
	video_init(boot_info);
	fb_clear();

	kprintf("========================================================================\n");
	kprintf("                     SIRIUS EDUCATION KERNEL x86_64                     \n");
	kprintf("========================================================================\n\n");

	kprintf("RAM %d MB\n",boot_info->MemoryMap.InstalledRAM/1024/1024);

	/*
	 * 3. Inicializar o PMM (Physical Memory Manager)
	 */
	kprintf("[INIT] Inicializando o Gestor de Memoria Fisica (PMM)...\n");
	pmm_init(boot_info);

	/*
	 * 4. Inicializar o VMM (Virtual Memory Manager) / Kernel Heap
	 */
	kprintf("[INIT] Inicializando o Gestor de Memoria Virtual (VMM) e Heap...\n");
	// vmm_init();

	/*
	 * 5. Alocar o CpuDataBlock do BSP (Core Principal)
	 */
	kprintf("[INIT] Alocando bloco de dados da CPU (CpuDataBlock) para o BSP...\n");

	/*
	 * 6. Preenche a GDT e o TSS dentro do cpu_blocks[0]
	 * 7. Executa a instrução LGDT apontando para cpu_blocks[0]->gdtr
	 */
	kprintf("[INIT] Configurando GDT e TSS globais...\n");

	/*
	 * 8. Configura o MSR GS_BASE do Core 0 para apontar para cpu_blocks[0]
	 */
	kprintf("[INIT] Configurando registador MSR GS_BASE...\n");

	/*
	 * 9. Inicializar a IDT Global
	 */
	kprintf("[INIT] Inicializando a Tabela de Descritores de Interrupcao (IDT)...\n");
	// idt_init();

	/*
	 * 10. ACPI
	 * 11. Drivers
	 * 12. VFS
	 * 13. Scheduler
	 * 14. IPC
	 * 15. Modules
	 */
	kprintf("[INIT] Inicializando ACPI, barramentos e drivers locais...\n");

	kprintf("\n========================================================================\n");
	kprintf("Sirius OS carregado com sucesso. Sistema pronto.\n");
	kprintf("========================================================================\n");

	/*
	 * Loop de paragem segura do Kernel
	 */
	for (;;)
	{
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
	}
}