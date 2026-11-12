#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/arch/x86_64/mm/paging.h>
#include <kernel/arch/x86_64/cpu/ipi.h>
#include <kernel/klib.h>

// Contador atómico para garantir que todas as CPUs concluíram o flush
static volatile int tlb_flush_pending = 0;

static inline uint64_t irq_save_disable(void) {
    uint64_t rflags;
    __asm__ __volatile__(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(rflags)
        :
        : "memory"
    );
    return rflags;
}

static inline void irq_restore(uint64_t rflags) {
    if (rflags & (1 << 9)) {
        __asm__ __volatile__("sti" ::: "memory");
    }
}

void send_ipi(uint32_t apic_id, uint8_t vector) {
    uint64_t rflags = irq_save_disable();

    // 1. Espera o LAPIC estar livre (bit 12 = Delivery Status)
    while (lapic_read_reg(0x300) & (1 << 12)) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    // 2. Define o LAPIC ID de destino nos bits 24-31 do ICR High (0x310)
    lapic_write_reg(0x310, (apic_id & 0xFF) << 24);

    // 3. Envia IPI: Fixed Delivery (000), Physical Mode (0), Assert (1 << 14), Edge (0)
    lapic_write_reg(0x300, (uint32_t)vector | (1 << 14));

    // 4. Espera o envio ser aceite pelo barramento APIC
    while (lapic_read_reg(0x300) & (1 << 12)) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    irq_restore(rflags);
}

// Handler chamado pelo stub assembly da IDT quando o vetor 0xF0 dispara
void handle_ipi_flush_tlb(void) {
    flush_tlb_local();
    
    // Confirma à CPU emissora que o TLB desta CPU foi limpo
    __sync_sub_and_fetch(&tlb_flush_pending, 1);
}

// Envia IPI de flush TLB para todas as outras CPUs ativas
void smp_flush_tlb_all(void) {
    
    // Obtém o APIC ID da CPU atual
    uint32_t current_apic_id = get_current_cpu_id();

    int targets = 0;

    // Conta quantas CPUs online existem além da atual
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cpu_blocks[i] != NULL && cpu_blocks[i]->lapic_id != current_apic_id) {
            targets++;
        }
    }

    if (targets > 0) {
        tlb_flush_pending = targets;
        __sync_synchronize();

        for (int i = 0; i < MAX_CPUS; i++) {
            if (cpu_blocks[i] != NULL && cpu_blocks[i]->lapic_id != current_apic_id) {
                send_ipi(cpu_blocks[i]->lapic_id, IPI_FLUSH_TLB);
            }
        }

        // Espera todas as CPUs confirmarem o flush (com timeout de segurança)
        uint64_t timeout = 10000000;
        while (tlb_flush_pending > 0 && timeout > 0) {
            __asm__ __volatile__("pause" ::: "memory");
            timeout--;
        }
    }

    // Limpa o TLB da CPU atual
    flush_tlb_local();
}