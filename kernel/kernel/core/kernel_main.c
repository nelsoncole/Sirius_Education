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
 *  Modified Date: 15/09/2026
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
#include <kernel/arch/x86_64/kapi/acpi.h>
#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/cpu/ioapic.h>
#include <kernel/arch/x86_64/cpu/smp.h>
#include <kernel/arch/x86_64/kapi/msi.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/drivers/bus/pci.h>
#include <kernel/drivers/char/keyboard.h>
#include <kernel/drivers/char/mouse.h>
#include <kernel/drivers/storage/ahci.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/drivers/storage/partitions.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/dev/vfs_tty.h>
#include <kernel/fs/fat/fat32.h>
#include <kernel/klib.h>
#include <kernel/kernel/sched/elf.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/kernel/net/socket.h>

extern void test(void);
extern void tty_emulator_thread(void);
extern void tty_keyboard_bridge_thread();

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
    pci_bus_init();
    msi_init();

    // 14. Inicializa as tabelas globais de dispositivos de bloco
    block_subsystem_init();
    // 15. Inicializa o VFS (Cria a raiz virtual '/' em RAM)
    vfs_init();  /* Cria a raiz '/' em memória RAM */
    // Cria uma pasta na raiz usando a função pública de acesso
    vfs_node_t *root = vfs_get_root();
    if (root)
    {
        vfs_mkdir(root, "mnt", 0x01FF); // Cria pasta na RAM
        vfs_node_t *mnt = vfs_path_to_node("/mnt");
        if (mnt)
        {
            vfs_mkdir(mnt, "hd", 0x01FF); // Cria /mnt/hd na RAM
        }
    }
    else
    {
        kprintf("[BOOT] ERRO CRÍTICO: Falha catastrofica de alocacao no heap do Kernel.\n");
        kprintf("[BOOT] Nao foi possivel instanciar a estrutura do no raiz '/' do VFS.\n");
        kprintf("[BOOT] Sistema travado em seguranca para impedir panico de hardware.\n");
        while (1)
        {
            __asm__ __volatile__("hlt");
        }
    }
    
    tty_init();  /* Prepara os buffers circulares e spinlocks do TTY */
    tty_vfs_init();  /* Cria e fixa o nó global da TTY no VFS */

    // 16. Driveres
    // 16.1
    keyboard_ps2_init();
    // 16.2
    mouse_ps2_init();
    // 16.3. Inicializa o controlador físico (ex: AHCI/SATA ou IDE)
    // NOTA: O driver AHCI DEVE registar o HD/SSD bruto no catálogo via 'register_block_device'
    // dando-lhe o nome literal de "ahci%d".
    ahci_driver_init();

    // 17. Regista o Driver do Sistema de Ficheiros FAT32 no catálogo do VFS
    fat32_init();

    init_socket();

    // Aqui vamos inicializar as particoes de disco
    // vamos identificar o nome da particao de boot
    vfs_init_partitions();
    if (g_boot_partition_name[0] == '\0') 
    {
        kprintf("[BOOT] Erro Fatal: Partição física de boot não encontrada por assinatura.\n");
        kprintf("[BOOT] Kernel travado em segurança para impedir falhas de hardware.\n");
        while (1) {
            __asm__ __volatile__("hlt");
        }
    }

    kprintf("[BOOT] Montando a partição de boot como raiz do VFS...\n");
    //int status = vfs_mount(g_boot_partition_name, "/", "fat32");
    int status = vfs_mount(g_boot_partition_name, "/mnt/hd", "fat32");
    if (status != 0)
    {
        kprintf("[BOOT] Erro Fatal: Falha crítica ao montar a partição '%s' usando o driver 'fat32' (Código: %d).\n",
                g_boot_partition_name, status);
        kprintf("[BOOT] Kernel travado em segurança para impedir pânicos de hardware no VFS.\n");

        while (1)
        {
            __asm__ __volatile__("hlt");
        }
    }

    /* 3. Cria a thread mestre passando o topo da stack devidamente blindado */
    /*thread_t *test_th = thread_create(test, 0);
    if (!test_th)
    {
        kprintf("[Thread] Erro: Falha ao criar a thread (test)\n");
    }*/

    /*
	 * 16. Modules
	 */

	kprintf("\n========================================================================\n");
	kprintf("Sirius OS carregado com sucesso. Sistema pronto.\n");
	kprintf("========================================================================\n");

    /* 
     * Lança as duas Kthreads de segundo plano:
     * 1. A do Emulador (que consome o tty_pop_output e faz kprintf)
     * 2. A do Teclado (que consome o scancode bruto, traduz e injeta na TTY)
     */
    thread_create(tty_emulator_thread, 0);
    thread_create(tty_keyboard_bridge_thread, 0);
    /*
     * ============================================================================
     * ARRANQUE DO PROCESSO INICIAL DO ESPAÇO DE UTILIZADOR (INIT / USER.ELF)
     * ============================================================================
     * A responsabilidade do Kernel cessa na inicialização do Hardware e do VFS. 
     * Daqui em diante, o controlo do ecossistema é delegado ao executável nativo
     * '/System/user.elf' em Ring 3, que criará o ambiente do utilizador (Shell).
     * 
     * DADOS VITAIS TRANSFERIDOS VIA ARGC/ARGV PARA A CRIAÇÃO DO AMBIENTE:
     * ----------------------------------------------------------------------------
     * 1. Origem de Boot (argv[1]): Passa o nome do volume ativo (ex: g_boot_partition_name)
     *    para que as aplicações saibam de onde ler ficheiros de configuração secundários.
     * 
     * 2. Modo de Operação (argv[2]): Sinaliza o estado do arranque (ex: "vga_mode", 
     *    "text_mode", "safe_mode" ou "single_user") orientando a Shell sobre se deve 
     *    ou não carregar interfaces gráficas complexas.
     * 
     * 3. Terminal TTY Alvo (argv[3]): Especifica qual a porta serial ou console virtual
     *    ativa (ex: "/dev/tty0") para direcionar os descritores padrões (stdout/stdin).
     * 
     * 4. Resolução de Ecrã (argv[4]): Passa a largura e altura detetadas pela UEFI
     *    (ex: "1024x768") para que as ferramentas do utilizador alinhem o texto perfeitamente.
     * ============================================================================
     */

    // Buffer local na RAM para armazenar a string de resolução formatada (ex: "1024x768")
    char uefi_res_str[32];
    memset(uefi_res_str, 0, sizeof(uefi_res_str));
    if (g_boot_info != NULL) {
        ksprintf(uefi_res_str, "%ux%u", g_boot_info->Graphics.Width, g_boot_info->Graphics.Height);
    } else {
        // Fallback de segurança académica caso o bloco g_boot_info falhe
        strncpy(uefi_res_str, "800x600", sizeof(uefi_res_str) - 1);
    }

    // MONTAGEM DINÂMICA DOS ARGUMENTOS:
    int init_argc = 5;
    char* init_argv[] = {
        "/System/user.elf",          // argv[0]: Caminho do executável
        g_boot_partition_name,       // argv[1]: Ex: "ahci0.1" (Origem de persistência)
        "text_mode",                 // argv[2]: Modo gráfico/texto base
        "/dev/tty0",                 // argv[3]: Terminal padrão do sistema
        uefi_res_str                 // argv[4]: Resolução de tela REAL e dinâmica da UEFI!
    };

    kprintf("[BOOT] Lancando o processo mestre de User Space '/System/user.elf'...\n");
    elf_load_and_create_process("/mnt/hd/System/user.elf", init_argc, init_argv, 0);

    vfs_print_tree("/");
    
    // Liga o barramento local de interrupções com segurança
    __asm__ __volatile__("sti");

	/*
     * ============================================================================
     * ESTACIONAMENTO SEGURO DOS NÚCLEOS (IDLE STATE)
     * Transita o processador para o loop de baixo consumo. O núcleo permanece
     * operacional e reativo às interrupções do Scheduler do Sirius_Education.
     * ============================================================================
     */
    cpu_idle();
}