/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: acpi.h
 *    Description: Estruturas de controlo e protótipos para o ACPI Parser
 *                 (XSDT/MADT) sintonizado em ambiente de 64-bits.
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

#ifndef _ACPI_H_
#define _ACPI_H_

#include <kernel/boot_info.h>

/* ============================================================================
 * ASSINATURAS DAS TABELAS ACPI (PADRÃO INTEL/AMD)
 * ============================================================================
 * Cada macro define a string de 4 caracteres identificadora da tabela.
 * ============================================================================
 */
#define ACPI_SIG_MADT   "APIC"      // Multiple APIC Description Table (LAPIC, IOAPIC, Cores)
#define ACPI_SIG_FADT   "FACP"      // Fixed ACPI Description Table (Gestão de energia/Reset)
#define ACPI_SIG_HPET   "HPET"      // High Precision Event Timer (Temporizador de alta precisão)
#define ACPI_SIG_MCFG   "MCFG"      // PCI Express Memory Mapped Configuration (Barramento PCIe)
#define ACPI_SIG_SSDT   "SSDT"      // Secondary System Description Table (Definições extras de ACPI)
#define ACPI_SIG_SBST   "SBST"      // Smart Battery Specification Table (Controlo de baterias)
#define ACPI_SIG_WAET   "WAET"      // Windows ACPI Emulated Devices Table (Otimizações de hypervisor)


// Cabeçalho padrão presente em todas as tabelas SDT do ACPI
typedef struct {
    unsigned char signature[4];    // Assinatura identificadora da tabela (ex: "XSDT")
    unsigned int  length;           // Tamanho total da tabela em bytes
    unsigned char revision;         // Versão da especificação da tabela
    unsigned char checksum;         // Validação matemática de integridade
    unsigned char oem_id[6];        // ID do fabricante original
    unsigned char oem_table_id[8];  // ID da tabela do fabricante
    unsigned int  oem_revision;      // Versão do fabricante
    unsigned int  creator_id;        // ID do utilitário de criação
    unsigned int  creator_revision;  // Versão do utilitário de criação
} __attribute__((packed)) acpi_sdt_header_t;

// Estrutura do RSDP v2 (Root System Description Pointer) de 64-bits
typedef struct {
    unsigned char signature[8];     // Assinatura "RSD PTR "
    unsigned char checksum;
    unsigned char oem_id[6];
    unsigned char revision;
    unsigned int  rsdt_address;     // Endereço antigo de 32 bits (ACPI 1.0)
    unsigned int  length;           // Tamanho da estrutura estendida
    unsigned long xsdt_address;     // Endereço físico de 64 bits da tabela XSDT
    unsigned char extended_checksum;
    unsigned char reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

/* Protótipos Globais */
void  acpi_init(BOOT_INFO *boot_info);
void* acpi_find_table(const char *signature);

unsigned int acpi_pm_read(void);

#endif // _ACPI_H_
