/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ioapic.c
 *    Description: Inicialização e roteamento de interrupções de hardware externos
 *                 através do I/O Advanced Programmable Interrupt Controller (IOAPIC).
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

#include <kernel/arch/x86_64/cpu/ioapic.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/lib/stdio.h>

// Ponteiro virtual global para os registadores MMIO de base do IOAPIC
static volatile uint32_t *g_ioapic = 0;

void* vmm_map_device(unsigned long phys_addr, unsigned long size);

/*
 * Escreve num registador interno do IOAPIC usando o esquema Select/Window
 */
static void ioapic_write(uint8_t reg, uint32_t value) {
    g_ioapic[IOAPIC_REG_SEL / 4] = reg;
    g_ioapic[IOAPIC_REG_WIN / 4] = value;
}

/*
 * Lê um registador interno do IOAPIC usando o esquema Select/Window
 */
static uint32_t ioapic_read(uint8_t reg) {
    g_ioapic[IOAPIC_REG_SEL / 4] = reg;
    return g_ioapic[IOAPIC_REG_WIN / 4];
}

/*
 * Configura o roteamento de uma IRQ física para um vetor da IDT e um núcleo alvo
 * 
 * Parâmetros:
 *   irq:    O pino físico de hardware (ex: IRQ 1 = Teclado PS/2)
 *   apic_id:O ID do Local APIC que vai receber a interrupção (ex: 0 para o BSP)
 *   vector: O vetor destino na IDT (deve ser >= 32, fora da zona de exceções)
 */
void ioapic_set_irq(uint8_t irq, uint64_t apic_id, uint8_t vector) {
    // Cada pino usa 2 registadores sequenciais começando em 0x10 + (irq * 2)
    uint8_t reg_low = IOAPIC_REDTBL + (irq * 2);
    uint8_t reg_high = reg_low + 1;

    /*
     * Parte Baixa (Bits 0..31):
     * Bits 0..7   -> O vetor destino na IDT
     * Bit 16      -> Interrupt Mask (0 = Ativo/Desmascarado, 1 = Mascarado)
     */
    uint32_t low_flags = vector & 0xFF; // Define o vetor na IDT e limpa a máscara (Bit 16 = 0)

    /*
     * Parte Alta (Bits 32..63):
     * Bits 56..63 -> Destinatário (O Local APIC ID do Core que receberá a interrupção)
     */
    uint32_t high_flags = (apic_id & 0xFF) << 24;

    // Grava as alterações no hardware
    ioapic_write(reg_low, low_flags);
    ioapic_write(reg_high, high_flags);
}

/*
 * Inicializa o chip IOAPIC mapeando a memória e desativando as IRQs legadas
 */
void ioapic_init(void) {
    kprintf("[IOAPIC] Mapeando registadores de IO em 0x%lX...\n", IOAPIC_PHYS_DEFAULT);

    // Mapeia a página de 4 KB na janela de 512 GB com Cache Disable (0x1B)
    g_ioapic = (volatile uint32_t *) vmm_map_device(IOAPIC_PHYS_DEFAULT, 4096);

    if (!g_ioapic) {
        kernel_panic("[IOAPIC ERRO CRITICO] Falha ao projetar MMIO do IOAPIC!\n");
        for(;;);
    }

    // Lê a versão e o número máximo de pinos de interrupção (Redirection Entries)
    uint32_t version_reg = ioapic_read(IOAPIC_VER);
    uint32_t max_interrupts = ((version_reg >> 16) & 0xFF) + 1;

    kprintf("[IOAPIC] Chip detectado. Versao: 0x%X, Pinos de interrupcao suportados: %u\n", 
            version_reg & 0xFF, max_interrupts);

    /*
     * MASCARAMENTO DE SEGURANÇA INICIAL
     * Desativamos todos os pinos de redirecionamento por padrão para limpar resíduos.
     * Escrever 0x10000 ativa o bit 16 (Masked), silenciando o pino.
     */
    for (uint32_t i = 0; i < max_interrupts; i++) {
        ioapic_write(IOAPIC_REDTBL + (i * 2), 0x10000);
        ioapic_write(IOAPIC_REDTBL + (i * 2) + 1, 0);
    }

    kprintf("[IOAPIC] Configuração global do barramento concluída com sucesso.\n");
}
