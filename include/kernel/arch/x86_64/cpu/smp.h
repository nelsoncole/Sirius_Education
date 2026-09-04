/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: smp.h
 *    Description: Estruturas de controle da tabela MADT (Multiple APIC 
 *                 Description Table) e protótipos de inicialização SMP.
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

#ifndef _SMP_H_
#define _SMP_H_

#include <kernel/lib/stdint.h>
#include <kernel/drivers/bus/acpi.h>
#include <kernel/boot_info.h>

/* 
 * Estrutura Base da Tabela MADT ("APIC")
 */
typedef struct {
    acpi_sdt_header_t header;       // Cabeçalho ACPI padrão
    uint32_t lapic_address;         // Endereço físico do LAPIC (geralmente 0xFEE00000)
    uint32_t flags;                 // Flags globais (Bit 0 = PCAT Compatível)
} __attribute__((packed)) acpi_madt_t;

/*
 * Cabeçalho genérico das sub-tabelas dinâmicas da MADT
 */
typedef struct {
    uint8_t type;                   // 0 = Local APIC, 1 = I/O APIC, 2 = ISO...
    uint8_t length;                 // Tamanho desta sub-tabela em bytes
} __attribute__((packed)) madt_entry_header_t;

/*
 * Sub-tabela Tipo 0: Processor Local APIC
 * Representa cada CPU/Thread física disponível no hardware.
 */
typedef struct {
    madt_entry_header_t header;
    uint8_t  acpi_processor_id;     // ID do Processador pelo ACPI
    uint8_t  apic_id;               // ID real do Local APIC (O alvo dos IPIs)
    uint32_t flags;                 // Bit 0 = Enabled, Bit 1 = Online Capable
} __attribute__((packed)) madt_lapic_entry_t;

/* Protótipos Globais */
void smp_init(BOOT_INFO *boot_info);

#endif // _SMP_H_
