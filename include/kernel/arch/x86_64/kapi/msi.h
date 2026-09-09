/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: msi.h
 *    Description: Interface de Abstração de Hardware para gestão e alocação 
 *                 dinâmica de Interrupções Sinalizadas por Mensagem (MSI).
 *                 Provê o mapeamento vetorial direto nativo de 64 bits para 
 *                 o subsistema Local APIC, isolando a infraestrutura de 
 *                 drivers de bloco e periféricos PCI de alto desempenho.
 * 
 *         Author: Nelson Cole
 *   Created Date: 08/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 08/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _MSI_H_
#define _MSI_H_

#include <kernel/drivers/bus/pci.h>

/* Limite estendido do hardware para suportar as 48 GSIs do IOAPIC moderno */
#define MAX_MSI_PINS    32

/* Definição do protótipo que o handler de interrupção do driver deve seguir */
typedef void (*msi_handler_t)(void);

/* 
 * Vetor global estendido exportado para o Kernel. 
 * Sincronizado para usar o tipo msi_handler_t definido na KAPI.
 */
extern msi_handler_t fnvetors_handler_msi[MAX_MSI_PINS];

/* Rotinas Públicas do Subsistema MSI */
int apic_send_msi(pci_device_t *dev, void (*fuc)(void));

/**
 * Inicializa o subsistema MSI.
 * Zera completamente a tabela de callbacks globais para garantir 
 * um estado limpo antes do registo de drivers de alta performance.
 */
void msi_init(void);


#endif /* _MSI_H_ */