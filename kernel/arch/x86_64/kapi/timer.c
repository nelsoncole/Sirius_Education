/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: timer.c
 *    Description: Subsistema de gestão de tempo de alta precisão (Delays).
 *                 Implementa atrasos por espera ativa (busy-wait) baseados
 *                 no Invariant TSC (Time-Stamp Counter) para x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/lib/stdint.h>
#include <kernel/arch/x86_64/kapi/timer.h>

uint64_t g_tsc_hz;

// Importa a função que lê o ACPI PM Timer que nos mostraste antes
extern unsigned int acpi_pm_read(void);

void timer_init(void) {
    uint32_t acpi_start = acpi_pm_read();
    
    // Teste de segurança: Se a porta devolver apenas 0xFFFFFFFF ou 0,
    // significa que o ACPI PM Timer não está acessível.
    if (acpi_start == 0xFFFFFFFF || acpi_start == 0) {
        // Fallback de emergência: assume uma frequência padrão (ex: 2.0 GHz)
        // para o kernel não travar no boot.
        g_tsc_hz = 2000000000ULL; 
        return;
    }

    uint64_t tsc_start = read_tsc();
    uint32_t current_ticks = 0;

    while (current_ticks < 35795) {
        uint32_t acpi_now = acpi_pm_read();
        
        // Tratamento correto de máscara para contadores ACPI de 24 bits
        // Isto resolve matematicamente o overflow do registo do chipset
        current_ticks = (acpi_now - acpi_start) & 0x00FFFFFF;

        __asm__ volatile("pause");
    }
    
    uint64_t tsc_end = read_tsc();
    g_tsc_hz = (tsc_end - tsc_start) * 100;
}

// Atraso em Nanossegundos (1s = 1.000.000.000 ns)
void ndelay(uint64_t nsecs) {
    uint64_t inicio = read_tsc();
    // Matemática pura em 64 bits (Seguro até ~4.6 segundos num CPU de 4GHz)
    uint64_t ciclos_a_esperar = (nsecs * g_tsc_hz) / 1000000000ULL;
    
    while ((read_tsc() - inicio) < ciclos_a_esperar) {
        __asm__ volatile("pause");
    }
}

// Atraso em Microssegundos (1s = 1.000.000 us)
void udelay(uint64_t usecs) {
    uint64_t inicio = read_tsc();
    uint64_t ciclos_a_esperar = (usecs * g_tsc_hz) / 1000000ULL;
    
    while ((read_tsc() - inicio) < ciclos_a_esperar) {
        __asm__ volatile("pause");
    }
}

// Atraso em Milissegundos (1s = 1.000 ms)
void mdelay(uint64_t msecs) {
    uint64_t inicio = read_tsc();
    // Evita divisão pesada convertendo o cálculo para ciclos por milissegundo
    uint64_t ciclos_a_esperar = msecs * (g_tsc_hz / 1000ULL);
    
    while ((read_tsc() - inicio) < ciclos_a_esperar) {
        __asm__ volatile("pause");
    }
}