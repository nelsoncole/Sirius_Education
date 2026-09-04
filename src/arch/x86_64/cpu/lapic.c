/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: lapic.c
 *    Description: Inicialização e controlo por software do Local APIC (LAPIC)
 *                 no processador atual via mapeamento seguro de MMIO.
 * 
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 31/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/drivers/bus/acpi.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/lib/stdio.h>
#include <kernel/lib/stdint.h>

// Ponteiro virtual global que aponta para a base do LAPIC mapeado no Higher-Half
static volatile uint32_t *g_lapic = 0;

/*
 * Função interna para escrever num registador do LAPIC (MMIO de 32 bits)
 */
static inline void lapic_write(uint32_t offset, uint32_t value) {
    g_lapic[offset / 4] = value;
}

/*
 * Função interna para ler um registador do LAPIC (MMIO de 32 bits)
 */
static inline uint32_t lapic_read(uint32_t offset) {
    return g_lapic[offset / 4];
}

/*
 * Retorna o ID único do LAPIC deste núcleo físico (Thread)
 */
unsigned int lapic_get_id(void) {
    if (!g_lapic) return 0;
    // Em arquiteturas x86_64 modernas, o ID reside nos 8 bits superiores (24..31)
    return (lapic_read(LAPIC_REG_ID) >> 24);
}

/*
 * Sinaliza o fim do processamento de uma interrupção de hardware (End of Interrupt)
 */
void lapic_eoi(void) {
    if (g_lapic) {
        lapic_write(LAPIC_REG_EOI, 0); // Qualquer escrita limpa a interrupção atual
    }
}

/*
 * Função global exportada para permitir que o smp.c envie comandos IPI
 * gravando diretamente nos registadores do barramento ICR.
 */
void lapic_write_reg(uint32_t offset, uint32_t value) {
    lapic_write(offset, value);
}

uint32_t lapic_read_reg(uint32_t offset) {
    return lapic_read(offset);
}

/*
 * Inicializa o Local APIC mapeando a região física e ativando o chip por software
 */
void lapic_init(void)
{
    kprintf("[LAPIC] Mapeando registadores físicos em 0x%lX...\n", LAPIC_PHYS_DEFAULT);

    /*
     * Mapeia a página de 4 KB do LAPIC no gerenciador VMM DEVICE.
     * Recebe um ponteiro virtual estável e imune a colisões com o Kernel.
     */
    g_lapic = (volatile uint32_t *) vmm_map_device(LAPIC_PHYS_DEFAULT, 4096);

    if (!g_lapic) {
        kernel_panic("[LAPIC ERRO CRITICO] Falha ao mapear os registadores do LAPIC!\n");
        for(;;);
    }

    /*
     * 1. LIMPEZA DE SEGURANÇA E MASCARAMENTO DO TIMOR DO LAPIC
     * Desativa temporariamente o timer nativo do APIC para evitar interrupções 
     * prematuras antes de o IOAPIC e o Scheduler estarem configurados.
     * Escrever 0x10000 ativa o bit 'Masked' (Desativado).
     */
    lapic_write(LAPIC_REG_LVT_TIMER, 0x10000);

    /*
     * 2. ATIVAÇÃO DO CONTROLADOR VIA SOFTWARE (SVR Register)
     * Lemos o estado atual do Spurious Vector Register (SVR).
     * Setamos o Bit 8 (0x100) -> APIC Software Enable (Ativa o chip real).
     * Setamos o Vetor de Spurious para 0xFF (Vetor 255 reservado na IDT).
     */
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    svr |= 0x100;     // Bit 8: Ativa o LAPIC
    svr |= 0xFF;      // Bits 0..7: Define o vetor de interrupções espúrias para 255
    lapic_write(LAPIC_REG_SVR, svr);

    /*
     * 3. CONFIGURAÇÃO DE ERROS DO HARDWARE LOCAL
     * Mapeia as interrupções de erro interno do chip APIC para o Vetor 254 (0xFE)
     * na IDT. Isto garante que falhas de barramento sejam capturadas.
     */
    lapic_write(LAPIC_REG_LVT_ERROR, 0xFE);

    /*
     * 4. LIMPAR ERROS PENDENTES
     * Duas escritas seguidas zeram qualquer registador de erro acumulado 
     * durante o boot da BIOS/UEFI.
     */
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_ESR, 0);

    // Sinaliza o fim de qualquer interrupção fantasma herdada do UEFI
    lapic_eoi();

    // Lê o ID real atribuído pela motherboard à CPU atual
    unsigned int id = lapic_get_id();
    uint32_t version = lapic_read(LAPIC_REG_VERSION) & 0xFF;

    kprintf("[LAPIC] Controlador ativado no BSP. ID Core: %u, Versao do Chip: 0x%X\n", id, version);
}


// Variável global para guardar o resultado da calibragem feita no primeiro núcleo
uint32_t g_lapic_ticks_calibrated = 0;

/*
 * Inicializa e calibra o Temporizador Local (LAPIC Timer) em modo periódico.
 * Aborda o método moderno de leitura do ACPI PM Timer para compatibilidade com o VMware.
 */
void lapic_timer_init(uint32_t hz)
{
    if (hz == 0 || !g_lapic) return;

    // 1. Configurar o LAPIC Timer com divisor por 16 (Necessário em todos os APs)
    lapic_write(LAPIC_REG_TDCR, 0x03);

    // Se o valor global ainda for 0, significa que esta é a PRIMEIRA chamada (executada pelo BSP)
    if (g_lapic_ticks_calibrated == 0) {
        kprintf("[TIMER] Calibrando LAPIC Timer via ACPI PM Timer (Modo Moderno)...\n");

        // 2. Preparar o intervalo exato de 10 milissegundos
        // Frequência do PM Timer: 3579545 Hz. Para 10ms: 3579545 / 100 = 35795 tiques
        uint32_t start_pm_tick = acpi_pm_read();
        uint32_t target_pm_ticks = 35795; // 10ms com base em 3.579545 MHz

        // 3. Setar o contador inicial do LAPIC para o valor máximo possível
        lapic_write(LAPIC_REG_TICR, 0xFFFFFFFF);

        // 4. Esperar o hardware terminar de contar os 10 milissegundos
        while ((acpi_pm_read() - start_pm_tick) < target_pm_ticks) {
            __asm__ __volatile__("pause"); // Otimização de hardware para loops de espera em Ring 0
        }

        // 5. Parar o temporizador local capturando quantos tiques ele decrementou
        g_lapic_ticks_calibrated = 0xFFFFFFFF - lapic_read(LAPIC_REG_TCCR);
        
        kprintf("[TIMER] Calibrado com sucesso pelo BSP.\n");
    }

    // 6. Configurar o Timer em Modo Periódico (Bit 17 ativo) acoplado ao Vetor 32 da IDT
    uint32_t lvt_timer = 0x20000 | 32;
    lapic_write(LAPIC_REG_LVT_TIMER, lvt_timer);

    /*
     * 7. DEFINIR O COEFICIENTE REAL DE VELOCIDADE
     * g_lapic_ticks_calibrated contém o total de oscilações em 10ms.
     * Multiplicar por 100 dá a velocidade por segundo (1000ms).
     * Dividindo pela frequência desejada (hz), achamos o valor cirúrgico para o hardware real!
     */
    uint32_t calibrated_ticks = (g_lapic_ticks_calibrated * 100) / hz;
    lapic_write(LAPIC_REG_TICR, calibrated_ticks);

    kprintf("[TIMER] Núcleo configurado: %u ticks por fatia de tempo a %u Hz.\n", calibrated_ticks, hz);
}


