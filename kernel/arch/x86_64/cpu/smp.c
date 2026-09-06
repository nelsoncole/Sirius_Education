/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: smp.c
 *    Description: Implementação do subsistema de Multiprocessamento Simétrico.
 *                 Efetua o parsing da MADT, copia o trampolim, injeta dados
 *                 e dispara a sequência de IPIs elétricos (INIT/STARTUP).
 *
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 04/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/smp.h>
#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/arch/x86_64/cpu/idt.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/mm/memory_map.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/drivers/bus/acpi.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/klib.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kernel/syscall/syscall.h>

// Endereço físico padrão de 16 bits exigido pela arquitetura para o Boot dos APs
#define TRAMPOLINE_PHYS_ADDRESS 0x8000UL

// Declaração cirúrgica de variáveis de bytes puros para o incbin
uint8_t *_binary_trampoline_start;

// Estrutura global da IDT para os APs carregarem localmente
extern idtr_t g_idtr;

/*
 * ============================================================================
 * SPINLOCK GLOBAL DE INICIALIZAÇÃO DE APs
 * 0 = Livre para o AP arrancar, 1 = AP ativo a configurar-se, bloqueia o BSP.
 * ============================================================================
 */
static volatile int g_smp_ap_gate = 0;

/* Contador atómico de núcleos online (O BSP começa em 1) */
static volatile uint32_t g_smp_cpus_online = 1;


/*
 * Ponto de Entrada mestre de 64-bits para onde todos os APs saltam após o trampolim.
 * Cada núcleo ganha o seu próprio fluxo de execução em paralelo.
 */
__attribute__((aligned(16)))
void segment_ap_main(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top)
{
    kprintf("[SMP] Nucleo %u (LAPIC ID: %u) Stack %lX!\n", cpu_id, lapic_id, stack_top);

    // 1. Inicializa o bloco de isolamento Per-CPU (GDT, TSS, MSR GS_BASE) do núcleo atual
    cpu_initialize_local(cpu_id, lapic_id, stack_top);

    // 2. Carrega a tabela global de interrupções neste núcleo físico
    __asm__ __volatile__("lidt %0" : : "m"(g_idtr));

    // 3. Inicializa e limpa o controlador Local APIC desta thread
    lapic_init();

    // 4. Ativa o relógio local a 100 Hz copiando o coeficiente estável do BSP
    lapic_timer_init(100);

    // 5. Inicializa o Scheduler para o AP (Core 1+)
	scheduler_init();

    /* 6. Programa os MSRs locais deste núcleo para suportar Syscalls */
    syscall_init();

    // 6. Liga o barramento local de interrupções com segurança
    __asm__ __volatile__("sti");

    kprintf("[SMP] Nucleo %u (LAPIC ID: %u) online e operando em Long Mode!\n", cpu_id, lapic_id);

    // Incrementa de forma atómica o número de CPUs prontos no sistema
    __atomic_add_fetch(&g_smp_cpus_online, 1, __ATOMIC_SEQ_CST);


    // Força a escrita na cache e RAM antes de libertar o BSP
    __asm__ volatile("mfence" ::: "memory");
    
    /*
     * ============================================================================
     * LIBERAÇÃO DO GATE LOCK
     * O AP atual concluiu com sucesso toda a inicialização crítica e libertou a 
     * memória virtual 0x8000. Avisa o BSP que o próximo AP já pode ser acordado.
     * ============================================================================
     */
    __atomic_clear(&g_smp_ap_gate, __ATOMIC_RELEASE);

    /*
     * ============================================================================
     * ESTACIONAMENTO SEGURO DOS NÚCLEOS (IDLE STATE)
     * Transita o processador para o loop de baixo consumo. O núcleo permanece
     * operacional e reativo às interrupções do Scheduler do Sirius_Education.
     * ============================================================================
     */
    cpu_idle();
}

/*
 * Executa o protocolo elétrico oficial de Boot (IPI Sequence) para um AP alvo
 */
// Função auxiliar essencial para monitorizar o barramento do LAPIC
static void lapic_wait_delivery(void) {
    // Aguarda enquanto o Bit 12 (Delivery Status) do ICR_LOW estiver a 1 (Ocupado)
    while (lapic_read_reg(LAPIC_REG_ICR_LOW) & (1 << 12)) {
        __asm__ __volatile__("pause" ::: "memory");
    }
}

static void smp_boot_ap(uint32_t cpu_id, uint8_t target_lapic_id, unsigned long bsp_cr3, uint8_t *trampoline_target)
{
    /*
     * ============================================================================
     * AQUISIÇÃO DO GATE LOCK (Sincronização BSP)
     * Antes de injetar dados em 0x8000, o BSP tranca o Gate. Se o AP anterior 
     * ainda estiver a iniciar, o BSP espera aqui para não corromper o trampolim.
     * ============================================================================
     */
    while (__atomic_test_and_set(&g_smp_ap_gate, __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    // Calcula o tamanho real do binário gerado pelo NASM
    unsigned long trampoline_size = (unsigned long)4096;

    // Limpa a página de lixo antes da carga
    memset(trampoline_target, 0, 4096);

    // Copia os bytes do trampolim de modos diretamente para a RAM física em 0x8000
    memcpy(trampoline_target, _binary_trampoline_start, trampoline_size);

    /*
     * 1. INJEÇÃO EM LOCAIS FIXOS E CONHECIDOS (64-bits)
     * Como usamos 'org 0x8000', as variáveis estão em offsets imutáveis por hardware:
     *   - ap_injected_cr3       reside em 0x8008 (início + 8)
     *   - ap_stack              reside em 0x8010 (início + 16)
     *   - ap_injected_c_handler reside em 0x8018 (início + 24)
     */
    uint64_t *injected_cr3 = (uint64_t *)(trampoline_target + 0x08);
    uint64_t *injected_stack = (uint64_t *)(trampoline_target + 0x10);
    uint32_t *injected_cpu_id = (uint32_t *)(trampoline_target + 0x18);
    uint32_t *injected_lapic_id = (uint32_t *)(trampoline_target + 0x1C);
    uint64_t *injected_c_handler = (uint64_t *)(trampoline_target + 0x20);

    // Injeta os dados mestres
    *injected_cr3 = (uint64_t)bsp_cr3;
    *injected_cpu_id = (uint32_t)cpu_id;
    *injected_lapic_id = (uint32_t)target_lapic_id & 0xFF;
    *injected_c_handler = (uint64_t)&segment_ap_main;

    /*
     * 2. ALOCAÇÃO DA PILHA ISOLADA PARA ESTA CPU SECUNDÁRIA
     * Cada CPU precisa do seu próprio Heap/Stack para não esmagar as outras.
     * Alocamos uma página limpa do Kernel Heap para o topo da pilha do AP.
     * (Ajuste ou substitua pelo seu alocador dinâmico kmalloc se necessário)
     *
     * SOLUÇÃO INDUSTRIAL:
     *      Pede 1 página de 4 KB livre da memória RAM física ao PMM.
     */
    unsigned long ap_stack_phys = pmm_alloc_page();
    if (ap_stack_phys == 0)
    {
        kernel_panic("[SMP ERRO] Nao ha RAM fisica livre para a pilha do AP!\n");
        for (;;);
    }
    uint64_t ap_stack_top = (uint64_t)vmm_map_device(ap_stack_phys, 4096) + 4096;
    *injected_stack = (uint64_t)ap_stack_top;

    
    kprintf("[SMP] Disparando pulso eletrico para o Core %u (LAPIC ID: %u)...\n", cpu_id, target_lapic_id);

    uint8_t boot_vector = (TRAMPOLINE_PHYS_ADDRESS / 4096) & 0xFF;
    uint32_t start_time;

    // Limpa registadores de erro
    lapic_write_reg(LAPIC_REG_ESR, 0);
    lapic_write_reg(LAPIC_REG_ESR, 0);

    /*
     * ============================================================================
     * FASE 1: INIT IPI (ASSERT)
     * Envia o sinal de Reset para o core alvo.
     * ============================================================================
     */
    lapic_wait_delivery(); // Garante barramento livre
    lapic_write_reg(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);
    
    // Padrão Intel: 0x4500 (Level=Assert, Delivery=INIT). O VMware exige este formato estável.
    lapic_write_reg(LAPIC_REG_ICR_LOW, 0x4500); 

    // ESPERAR ~10 milissegundos para estabilização elétrica do RESET do core
    start_time = acpi_pm_read();
    while ((acpi_pm_read() - start_time) < 35795) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    /*
     * ============================================================================
     * FASE 2: REMOVIDA / ADAPTADA (O erro do Deassert foi corrigido)
     * Em processadores modernos, não se faz Deassert para um ID específico.
     * Apenas aguardamos que o barramento envie o comando pendente.
     * ============================================================================
     */
    lapic_wait_delivery();

    /*
     * ============================================================================
     * FASE 3: STARTUP IPI #1 (SIPI #1)
     * Força o processador a acordar a partir do vetor especificado.
     * ============================================================================
     */
    lapic_write_reg(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);
    
    // Padrão Intel: 0x4600 (Level=Assert, Delivery=STARTUP) + Vetor
    lapic_write_reg(LAPIC_REG_ICR_LOW, 0x4600 | boot_vector); 

    // ESPERAR ~200 microssegundos
    start_time = acpi_pm_read();
    while ((acpi_pm_read() - start_time) < 716) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    /*
     * ============================================================================
     * FASE 4: STARTUP IPI #2 (SIPI #2)
     * Pulso redundante exigido por hardware físico e simuladores estritos como VMware.
     * ============================================================================
     */
    lapic_wait_delivery();
    lapic_write_reg(LAPIC_REG_ICR_HIGH, (uint32_t)target_lapic_id << 24);
    lapic_write_reg(LAPIC_REG_ICR_LOW, 0x4600 | boot_vector);

    // Aguarda o processamento final
    lapic_wait_delivery();

    // Delay de segurança de 1ms para o AP rodar as primeiras instruções de 16-bits
    start_time = acpi_pm_read();
    while ((acpi_pm_read() - start_time) < 3579) {
        __asm__ __volatile__("pause" ::: "memory");
    }
    
    kprintf("[SMP] Sinais enviados. Aguardando resposta do AP...\n");

    /* 
     * NOTA: O BSP NÃO liberta o lock aqui! O lock permanece trancado até que o AP 
     * execute 'segment_ap_main' e faça o clear do lock por si mesmo. Isto garante 
     * o isolamento absoluto e sequencial da inicialização.
     */
}

/*
 * Inicialização e varrimento global da topologia multiprocessador
 */
void smp_init(BOOT_INFO *boot_info)
{
    if(!boot_info) {
        kprintf("[SMP ERRO] Sem informacoes de boot! Operando em modo Uniprocessador.\n");
        return;
    }

    kprintf("[SMP] Iniciando mapeamento da topologia de nucleos...\n");

    /*
     * ============================================================================
     * INICIALIZAÇÃO DO GATE LOCK
     * O Zerar a variavel evita possiveis lock não definidos pelo software
     * ============================================================================
     */
    __atomic_clear(&g_smp_ap_gate, __ATOMIC_RELEASE);
    g_smp_cpus_online = 1;

    // Requisita a tabela MADT ao barramento ACPI usando o macro de assinatura
    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table(ACPI_SIG_MADT);
    if (!madt)
    {
        kprintf("[SMP ERRO] Tabela MADT nao encontrada! Operando em modo Uniprocessador.\n");
        return;
    }

    _binary_trampoline_start = (uint8_t *)KERNEL_TRAMPOLINE_VIRTUAL_BASE;
    // Mapeia a página física 0x8000 na janela de 512 GB com Cache Disable (0x1B)
    uint8_t *trampoline_target = (uint8_t *)vmm_map_device(TRAMPOLINE_PHYS_ADDRESS, 4096);

    // Captura o CR3/PML4 ativo no BSP atual
    unsigned long bsp_cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(bsp_cr3));

    // Obtém o ID do Local APIC do nosso BSP atual (Core 0)
    unsigned int bsp_apic_id = lapic_get_id();
    uint32_t active_cores_count = 1; // O BSP já está ativo

    // Localiza o início das sub-tabelas dinâmicas da MADT
    uint8_t *entry_ptr = (uint8_t *)((unsigned long)madt + sizeof(acpi_madt_t));
    uint8_t *madt_end = (uint8_t *)((unsigned long)madt + madt->header.length);

    // Varre o bloco dinâmico de memória em busca das CPUs físicas
    while (entry_ptr < madt_end)
    {
        madt_entry_header_t *header = (madt_entry_header_t *)entry_ptr;

        // Tipo 0 = Processor Local APIC (CPU física/lógica detectada)
        if (header->type == 0)
        {
            madt_lapic_entry_t *lapic_entry = (madt_lapic_entry_t *)entry_ptr;

            // Verifica se a CPU está habilitada por hardware (Bit 0 ativo nas flags)
            if (lapic_entry->flags & 0x01)
            {
                // Ignora se for o próprio BSP (Core 0) que já está ativo
                if (lapic_entry->apic_id == bsp_apic_id)
                {
                    entry_ptr += header->length;
                    continue;
                }

                kprintf("[SMP] Core Detectado -> ACPI Processor ID: %u, LAPIC ID: %u\n",
                        lapic_entry->acpi_processor_id, lapic_entry->apic_id);

                // EXCUÇÃO DO DISPARO ELÉTRICO CONTRA O ALVO!
                smp_boot_ap(active_cores_count, lapic_entry->apic_id, bsp_cr3, trampoline_target);

                active_cores_count++;
            }
        }

        // Avança o ponteiro pelo tamanho exato desta entrada para ler a seguinte
        entry_ptr += header->length;
    }

    /* 
     * BARREIRA DE INICIALIZAÇÃO SMP GLOBAL
     * O BSP aguarda ativamente até que TODOS os APs acordados tenham 
     * incrementado o contador 'g_smp_cpus_online'.
     */
    kprintf("[SMP] BSP aguardando a sincronizacao de todos os nucleos...\n");
    while (g_smp_cpus_online < active_cores_count)
    {
        __asm__ __volatile__("pause" ::: "memory");
    }

    kprintf("[SMP] Todos os %u nucleos sincronizados e prontos para o agendador!\n", g_smp_cpus_online);
}
