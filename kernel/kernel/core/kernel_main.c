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
 *  Modified Date: 11/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel.h>
#include <kernel/drivers/video/video.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/mm/heap.h>
#include <kernel/kernel/mm/pool.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/syscall/syscall.h>
#include <kernel/arch/x86_64/cpu/idt.h>
#include <kernel/drivers/bus/acpi.h>
#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/cpu/ioapic.h>
#include <kernel/arch/x86_64/cpu/smp.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/drivers/bus/pci.h>
#include <kernel/drivers/char/keyboard.h>
#include <kernel/drivers/char/mouse.h>
#include <kernel/drivers/storage/ahci.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/arch/x86_64/kapi/msi.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

extern void test_read_gpt_table(void);
extern void test_pool_reusability(void);

unsigned char user_program_binary[] = {

    /*
     * ============================================================
     * PROGRAMA USER RING 3 - x86_64
     *
     * Base virtual: 0x0000000000400000
     * ============================================================
     */

    /*
     * 0x400000
     *
     * jmp +14
     *
     * Salta para 0x400010
     */
    0xEB, 0x0E,

    /*
     * ============================================================
     * 0x400002
     *
     * "Hello Ring3!\n"
     * 13 bytes
     *
     * ============================================================
     */
    'H', 'e', 'l', 'l', 'o', ' ',
    'R', 'i', 'n', 'g', '3', '!', '\n',

    /*
     * Padding para o código começar em 0x400010
     */
    0x00,

    /*
     * ============================================================
     * 0x400010
     * CÓDIGO x86-64
     * ============================================================
     */

    /*
     * mov rax, 1
     *
     * RAX = SYS_WRITE (1)
     */
    0x48, 0xC7, 0xC0,
    0x01, 0x00, 0x00, 0x00,

    /*
     * mov rdi, 0x400002
     *
     * RDI = endereço da string
     */
    0x48, 0xC7, 0xC7,
    0x02, 0x00, 0x40, 0x00,

    /*
     * mov rsi, 13
     *
     * RSI = tamanho da string
     */
    0x48, 0xC7, 0xC6,
    0x0D, 0x00, 0x00, 0x00,

    /*
     * syscall (Executa o SYS_WRITE)
     */
    0x0F, 0x05,

    /*
     * ============================================================
     * ADICIONADO: SEÇÃO DE SAÍDA CONTROLADA (SYS_EXIT)
     * ============================================================
     */

    /*
     * mov rax, 3
     *
     * RAX = SYS_EXIT (Número da sua syscall de saída)
     */
    0x48, 0xC7, 0xC0,
    0x03, 0x00, 0x00, 0x00,

    /*
     * xor rdi, rdi (ou mov rdi, 0)
     *
     * RDI = 0 (Status code de saída com sucesso)
     */
    0x48, 0x31, 0xFF,

    /*
     * syscall (Executa o SYS_EXIT e destrói esta tarefa voluntariamente)
     */
    0x0F, 0x05};

/*
 * IMPORTANTE: Declara o rótulo do Assembly como um símbolo externo.
 * Usamos o tipo 'char' apenas para o C entender que é um endereço.
 */
extern char stack_top;
extern volatile int kprintf_spinlock;
extern uint32_t g_lapic_ticks_calibrated;
extern int bootverbose;

void kernel_main(BOOT_INFO *boot_info)
{
    kprintf_spinlock = 0;
    g_lapic_ticks_calibrated = 0;
    bootverbose = 0;

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
     * 5. Inicializando estruturas Per-CPU (CpuDataBlock) do BSP (Core Principal)
     * 6. Inicializar a IDT Global
     * 7. ACPI
     * 8. APIC
     * 9. IOAPIC
     * 10. SMP
     * 11. Scheduler
     * 12. PCB
     * 13. IPC
     * 14. Drivers
     * 15. VFS
     * 16. Modules
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

    kprintf("========================================================================\n");
    kprintf("                     SIRIUS EDUCATION KERNEL x86_64                     \n");
    kprintf("========================================================================\n\n");

    kprintf("RAM %d MB\n", boot_info->MemoryMap.InstalledRAM / 1024 / 1024);

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
    pool_init();

    /*
     * 5. Inicializando estruturas Per-CPU (CpuDataBlock) para o BSP
     *
     * cpu_id   = 0  (O BSP é sempre o primeiro núcleo)
     * lapic_id = 0  (Geralmente 0 no BSP, ou leia dinamicamente via registadores do APIC)
     * stack    = Passamos o topo da stack atual que o kernel está a usar com pilha real (16 KiB)
     * ============================================================================
     */
    kprintf("[CPU] Inicializando estruturas Per-CPU para o BSP (Core 0)...\n");

    uint64_t real_stack_top = (uint64_t)&stack_top;

    // Configura a GDT, TSS, IST e o MSR IA32_GS_BASE exclusivos do BSP
    cpu_initialize_local(0, 0, real_stack_top);

    kprintf("[SUCESSO] BSP configurado com stack em 0x%lx!\n", real_stack_top);

    /*
     * 6. Inicializar a IDT Global
     */
    kprintf("[INIT] Inicializando a Tabela de Descritores de Interrupcao (IDT)...\n");
    idt_init();

    // 7. ACPI (Faz o parse das tabelas RSDP, XSDT e localiza a tabela MADT)
    acpi_init(boot_info);

    // 8. Inicializar o Local APIC (LAPIC) no BSP
    // Mapeia o endereço 0xFEE00000 e ativa o tratamento de interrupções locais
    lapic_init();

    /*
     * ============================================================================
     * 8.1. INICIALIZAÇÃO E ARRANQUE DO LAPIC TIMER DO BSP
     * ============================================================================
     * Passamos a frequência de 100 Hz. Isto significa que o temporizador vai
     * calibrar-se via PIT e disparar o Vetor 32 exatamente 100 vezes por segundo,
     * criando uma fatia de tempo (tick) precisa de 10 milissegundos.
     * ============================================================================
     */
    kprintf("[SISTEMA] Iniciando o relogio mestre do Kernel...\n");
    lapic_timer_init(100);

    // 9. Inicializar o IOAPIC
    // Mapeia e roteia as interrupções de hardware (como teclado e timer) para a IDT
    ioapic_init();

    // 9.1. Programa os MSRs locais deste núcleo para suportar Syscalls
    syscall_init();

    // 10. Inicializar o SMP (Application Processors - APs)
    // Faz o parsing da MADT, acorda os restantes núcleos via IPIs (INIT/STARTUP)
    // e executa a cpu_initialize_local() dinamicamente em cada um deles!
    // bootverbose = 1;
    smp_init(boot_info);
    // bootverbose = 0;

    // 11. Inicializa o Scheduler para o BSP (Core 0)
    scheduler_init();

    // 12. PCB OK

    kprintf("[Kernel] Criando processo isolado a partir de binario bruto...\n");

    /*
     * Cria o processo, aloca o PML4 isolado, usa a scratch window para
     * injetar o array 'user_program_binary' na base 0x400000UL e cria a thread de Ring 3.
     */
    unsigned long user_program_size = sizeof(user_program_binary);
    process_t *app = process_create(user_program_binary, user_program_size, 0);

    if (!app)
    {
        kprintf("[Kernel] Erro critico: Falha ao carregar o aplicativo de teste.\n");
        while (1)
            ;
    }

    kprintf("[Kernel] Ativando multitasking. Transitando para Ring 3...\n");

    // 11.1. Liga o barramento local de interrupções com segurança
    //__asm__ __volatile__("sti");


    // 14. Driveres
    pci_bus_init();
    msi_init();
    keyboard_ps2_init();
    mouse_ps2_init();
    block_subsystem_init();
    ahci_driver_init();

    // 15. VFS
    vfs_init();


    /* 3. Cria a thread mestre passando o topo da stack devidamente blindado */
    thread_t *test_th = thread_create(test_read_gpt_table, 0);
    if (!test_th)
    {
        kprintf("[Thread] Erro: Falha ao criar a thread (test_read_gpt_table)\n");
    }

    test_th = thread_create(test_pool_reusability, 0);
    if (!test_th)
    {
        kprintf("[Thread] Erro: Falha ao criar a thread (test_pool_reusability)\n");
    }
    
    /*
	 * 13. IPC
	 * 14. Drivers
	 * 15. VFS
	 * 16. Modules
	 */



	kprintf("\n========================================================================\n");
	kprintf("Sirius OS carregado com sucesso. Sistema pronto.\n");
	kprintf("========================================================================\n");

	/*
     * ============================================================================
     * ESTACIONAMENTO SEGURO DOS NÚCLEOS (IDLE STATE)
     * Transita o processador para o loop de baixo consumo. O núcleo permanece
     * operacional e reativo às interrupções do Scheduler do Sirius_Education.
     * ============================================================================
     */
    cpu_idle();
}