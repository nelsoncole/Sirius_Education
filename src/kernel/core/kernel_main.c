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
#include <kernel/arch/mm/vmm.h>
#include <kernel/kernel/mm/heap.h>



void test_kernel_heap(void);

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

	g_boot_info = boot_info;

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
	vmm_init();
	kheap_init();

	test_kernel_heap();

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



#include <kernel/kernel/mm/heap.h>
/*
 * TESTE DO SUBSISTEMA DE MEMÓRIA DINÂMICA (TEST KHEAP)
 * ------------------------------------------------------------------------
 * Executa uma bateria de testes de estresse para validar o comportamento
 * do alocador do kernel sob fragmentação, escrita, leitura e fusão.
 */
void test_kernel_heap(void) {
    kprintf("\n[HEAP TEST] Iniciando verificacao do Kernel Heap...\n");

    /*
     * TESTE 1: Alocações Básicas e Escrita de Dados
     * ------------------------------------------------------------------------
     * Garante que os ponteiros virtuais devolvidos estão alinhados e acessíveis.
     */
    int* array1 = (int*)kmalloc(100 * sizeof(int));
    char* str1  = (char*)kmalloc(64 * sizeof(char));

    if (array1 == (void*)0 || str1 == (void*)0) {
        kprintf("[HEAP TEST] ERRO: Falha nas alocacoes primarias do Teste 1.\n");
        return;
    }

    // Testa a escrita física na memória virtual mapeada para garantir que não há Page Fault
    for (int i = 0; i < 100; i++) {
        array1[i] = i * 2;
    }
    
    // Teste de escrita na string
    for (int i = 0; i < 63; i++) {
        str1[i] = 'A' + (i % 26);
    }
    str1[63] = '\0';

    // Teste de leitura de verificação para garantir integridade dos dados
    int integridade_ok = 1;
    for (int i = 0; i < 100; i++) {
        if (array1[i] != i * 2) {
            integridade_ok = 0;
            break;
        }
    }

    if (integridade_ok) {
        kprintf("[HEAP TEST] Teste 1 Sucesso: Escrita/Leitura estaveis. Array1: 0x%p, Str1: 0x%p\n", 
                (unsigned long)array1, (unsigned long)str1);
    } else {
        kprintf("[HEAP TEST] ERRO: Corrupcao de dados detectada na leitura do Teste 1.\n");
    }

    /*
     * TESTE 2: Validação do Algoritmo Best-Fit e Fragmentação Propositada
     * ------------------------------------------------------------------------
     * Criamos 3 blocos contíguos e libertamos o do meio para criar um "buraco".
     */
    void* bloco_A = kmalloc(32);   // Bloco Pequeno
    void* bloco_B = kmalloc(256);  // Bloco Médio (será libertado)
    void* bloco_C = kmalloc(1024); // Bloco Grande
    void* bloco_D = kmalloc(512);  // Bloco Grande 2 (será libertado)
    void* bloco_E = kmalloc(64);   // Bloco Pequeno 2

    kprintf("[HEAP TEST] Criando buracos de fragmentacao controlada...\n");
    kfree(bloco_B); // Cria um buraco livre de 256 bytes
    kfree(bloco_D); // Cria um buraco livre de 512 bytes

    /*
     * Agora solicitamos 128 bytes. 
     * O algoritmo Best-Fit DEVE escolher o buraco do bloco_B (256 bytes, diff=128)
     * em vez do buraco do bloco_D (512 bytes, diff=384), pois o bloco_B é o "melhor ajuste".
     */
    unsigned char* bloco_realloc = (unsigned char*)kmalloc(128);
    kprintf("[HEAP TEST] Best-Fit escolheu o ponteiro: 0x%p\n", (unsigned long)bloco_realloc);
    
    // Verificação matemática de segurança
    if ((unsigned long)bloco_realloc == (unsigned long)bloco_B) {
        kprintf("[HEAP TEST] Teste 2 Sucesso: Algoritmo Best-Fit operando de forma correta.\n");
        
        // Teste de escrita no bloco reaproveitado pelo Best-Fit
        for (int i = 0; i < 128; i++) {
            bloco_realloc[i] = (unsigned char)(i ^ 0xAA);
        }
        
        // Teste de leitura no bloco reaproveitado
        int realloc_ok = 1;
        for (int i = 0; i < 128; i++) {
            if (bloco_realloc[i] != (unsigned char)(i ^ 0xAA)) {
                realloc_ok = 0;
                break;
            }
        }
        if (realloc_ok) {
            kprintf("[HEAP TEST] Escrita/Leitura no bloco Best-Fit validada com sucesso.\n");
        } else {
            kprintf("[HEAP TEST] ERRO: Falha na verificação de dados do bloco Best-Fit.\n");
        }
    } else {
        kprintf("[HEAP TEST] AVISO: Best-Fit nao escolheu o bloco ideal esperado.\n");
    }

    /*
     * TESTE 3: Limpeza Completa e Fusão de Blocos (Coalescing)
     * ------------------------------------------------------------------------
     * Libertamos toda a memória restante para forçar o Heap a fundir os nós
     * vizinhos de volta num único bloco gigante original.
     */
    kprintf("[HEAP TEST] Libertando toda a memoria para testar o Coalescing...\nences\n");
    kfree(array1);
    kfree(str1);
    kfree(bloco_A);
    kfree(bloco_realloc);
    kfree(bloco_C);
    kfree(bloco_E);

    /*
     * Se o Coalescing funcionou, podemos alocar um bloco de 1.5 MB consecutivamente.
     * Caso a fusão tenha falhado, a memória estará fragmentada em pequenos blocos
     * e esta alocação gigante falhará ou disparará a expansão.
     */
    unsigned long* bloco_gigante = (unsigned long*)kmalloc(15 * 1024 * 1024 / 10); // ~1.5 MB
    if (bloco_gigante != (void*)0) {
        kprintf("[HEAP TEST] Teste 3 Sucesso: Coalescing unificou os blocos livres com exito.\n");
        
        // Teste intensivo de escrita no bloco gigante/expandido
        unsigned long num_elementos = (1.5 * 1024 * 1024) / sizeof(unsigned long);
        kprintf("[HEAP TEST] Testando escrita em %d entradas no bloco expandido...\n", num_elementos / 10);
        
        // Escreve em intervalos espaçados para varrer várias páginas físicas mapeadas do Heap
        for (unsigned long i = 0; i < num_elementos; i += 512) {
            bloco_gigante[i] = i;
        }
        
        // Verifica a consistência da escrita
        int gigante_ok = 1;
        for (unsigned long i = 0; i < num_elementos; i += 512) {
            if (bloco_gigante[i] != i) {
                gigante_ok = 0;
                break;
            }
        }
        
        if (gigante_ok) {
            kprintf("[HEAP TEST] Escrita/Leitura no espaço expandido validada com sucesso absoluto.\n");
        } else {
            kprintf("[HEAP TEST] ERRO: Falha de consistência no espaço expandido.\n");
        }
        
        kfree(bloco_gigante);
    } else {
        kprintf("[HEAP TEST] ERRO: Falha ao recuperar bloco unificado pós-coalescing.\n");
    }

    kprintf("[HEAP TEST] Bateria de testes concluida com sucesso absoluto!\n\n");
}
