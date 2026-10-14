/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: acpi.c
 *    Description: Analisador ACPI de 64-bits escalável. Consome a janela
 *                 de MMIO gigante para mapear o RSDP e a árvore XSDT.
 *
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 10/10/2026
 * ============================================================================
 */

#include <kernel/arch/x86_64/kapi/acpi.h>
#include <kernel/kernel/mm/memory_map.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/lib/stdio.h>
#include <kernel/lib/string.h>

// Estruturas Oficiais Internas do ACPI MADT (APIC)
typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t  entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

typedef struct {
    madt_entry_header_t header;
    uint8_t  bus_source;   // 0 indica barramento ISA
    uint8_t  irq_source;   // O IRQ ISA original (ex: 11)
    uint32_t gsi_target;   // O pino GSI real mapeado no IOAPIC (ex: 11 ou 19)
    uint16_t flags;
} __attribute__((packed)) madt_int_override_t;

// Ponteiro global para a raiz das tabelas (Extended System Description Table)
static acpi_sdt_header_t *g_xsdt = 0;

// Define a variável global
unsigned int g_acpi_pm_timer_port = 0;

/*
 * Validação de Checksum matemática padrão do ACPI
 */
static int acpi_validate_checksum(void *addr, unsigned int length)
{
    unsigned char *bytes = (unsigned char *)addr;
    unsigned char sum = 0;
    for (unsigned int i = 0; i < length; i++)
    {
        sum += bytes[i];
    }
    return (sum == 0);
}

/*
 * Varre a árvore XSDT e mapeia dinamicamente a tabela solicitada via VMM
 */
void *acpi_find_table(const char *signature)
{
    if (!g_xsdt)
        return 0;

    unsigned int entries = (g_xsdt->length - sizeof(acpi_sdt_header_t)) / 8;
    unsigned long *table_pointers = (unsigned long *)((unsigned long)g_xsdt + sizeof(acpi_sdt_header_t));

    for (unsigned int i = 0; i < entries; i++)
    {
        unsigned long table_phys = table_pointers[i];
        if (table_phys == 0)
            continue;

        acpi_sdt_header_t *candidate = (acpi_sdt_header_t *)vmm_map_device(table_phys, sizeof(acpi_sdt_header_t));

        if (memcmp(candidate->signature, signature, 4) == 0)
        {
            void *final_table_address = vmm_map_device(table_phys, candidate->length);

            kprintf("[ACPI] Tabela '%s' mapeada com sucesso em virtual: 0x%lX\n",
                    signature, (unsigned long)final_table_address);

            return final_table_address;
        }
    }

    kprintf("[ACPI AVISO] Tabela '%s' nao foi localizada no barramento XSDT.\n", signature);
    return 0;
}

/*
 * Resolve e extrai de forma definitiva o mapeamento de interrupcoes ISA/PCI 
 * lendo a tabela MADT do ACPI. Resolve o problema de desvios (como 11 para 19).
 */
unsigned int acpi_get_ioapic_irq(unsigned int isa_irq)
{
    // Localiza a tabela MADT no barramento usando a assinatura padrão "APIC"
    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (!madt)
    {
        // Se a tabela não existir, faz fallback linear de segurança de 1:1
        return isa_irq; 
    }

    uintptr_t entry_ptr = (uintptr_t)madt->entries;
    uintptr_t end_ptr = (uintptr_t)madt + madt->header.length;

    // Varre todas as sub-entradas da tabela MADT
    while (entry_ptr < end_ptr)
    {
        madt_entry_header_t *entry = (madt_entry_header_t *)entry_ptr;
        
        // Tipo 2: Interrupt Source Override (Exceções de roteamento da motherboard)
        if (entry->type == 2)
        {
            madt_int_override_t *override = (madt_int_override_t *)entry_ptr;
            
            // Verifica se a exceção se aplica ao barramento ISA (bus 0) e ao IRQ pretendido
            if (override->bus_source == 0 && override->irq_source == isa_irq)
            {
                unsigned int resolved_gsi = override->gsi_target;
                kprintf("[ACPI] Roteamento de Interrupcao detetado: ISA IRQ %d -> GSI %d\n", 
                        isa_irq, resolved_gsi);
                return resolved_gsi; // Retorna 19 no ICH9 ou 11 se houver desvio
            }
        }

        // Avança com segurança para a próxima entrada baseando-se no tamanho lido por hardware
        if (entry->length == 0) break; // Evita loop infinito caso a tabela esteja corrompida
        entry_ptr += entry->length;
    }

    // Se nenhuma exceção foi declarada no ACPI, o mapeamento é direto (1:1)
    return isa_irq;
}

/*
 * Inicializa o subsistema ACPI efetuando o mapeamento do RSDP e da XSDT raiz
 */
void acpi_init(BOOT_INFO *boot_info)
{
    kprintf("[ACPI] Inicializando parser atraves do RSDP do UEFI...\n");

    if (boot_info->RsdpAddress == 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] O Loader UEFI nao transmitiu o endereco do ACPI!\n");
        for (;;);
    }

    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)vmm_map_device(boot_info->RsdpAddress, sizeof(acpi_rsdp_t));

    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] Assinatura 'RSD PTR ' corrompida ou invalida!\n");
        for (;;);
    }

    kprintf("[ACPI] RSDP v%d validado com sucesso em 0x%lX (OEM: %.6s).\n",
            rsdp->revision, (unsigned long)rsdp, rsdp->oem_id);

    if (rsdp->revision < 2 || rsdp->xsdt_address == 0)
    {
        kernel_panic("[ACPI ERRO CRITICO] Hardware obsoleto: Sem suporte a tabelas XSDT de 64-bits!\n");
        for (;;);
    }

    acpi_sdt_header_t *header_tmp = (acpi_sdt_header_t *)vmm_map_device(rsdp->xsdt_address, sizeof(acpi_sdt_header_t));
    g_xsdt = (acpi_sdt_header_t *)vmm_map_device(rsdp->xsdt_address, header_tmp->length);

    if (!acpi_validate_checksum(g_xsdt, g_xsdt->length))
    {
        kernel_panic("[ACPI ERRO CRITICO] Checksum invalido ou corrompido na tabela raiz XSDT!\n");
        for (;;);
    }

    kprintf("[ACPI] Descobrindo a porta do PM Timer via tabela FADT...\n");

    unsigned char *fadt = (unsigned char *)acpi_find_table("FACP");

    if (fadt != 0)
    {
        g_acpi_pm_timer_port = *(unsigned int *)(fadt + 76);
        kprintf("[ACPI] PM Timer localizado por hardware na porta: 0x%X\n", g_acpi_pm_timer_port);
    }
    else
    {
        kprintf("[ACPI ERRO] Tabela FADT nao encontrada. Fallback para porta padrao.\n");
        g_acpi_pm_timer_port = 0x408; 
    }

    kprintf("[ACPI] Raiz XSDT montada com sucesso em 0x%lX (%u bytes).\n",
            (unsigned long)g_xsdt, g_xsdt->length);
}

/*
 * Método Moderno: Lê o ACPI Power Management Timer (PM Timer).
 */
unsigned int acpi_pm_read(void) {
    unsigned int value;
    unsigned short port = (g_acpi_pm_timer_port != 0) ? (unsigned short)g_acpi_pm_timer_port : 0x408;
    
    __asm__ __volatile__("inl %%dx, %0" : "=a"(value) : "d"(port));
    return value;
}