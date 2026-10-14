/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: net_core.c
 *    Description: Módulo Driver Dinâmico (LKM) para as placas de rede.
 * 
 *         Author: Nelson Cole
 *   Created Date: 10/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kmods/kmod.h>
#include <kernel/drivers/bus/pci.h>
#include <kernel/drivers/net/net_core.h>
#include <kernel/klib.h>

// Estrutura para mapear o Hardware ao Driver correto através de Vendor e Device ID
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    const char *driver_name;
} pci_net_driver_t;

static const pci_net_driver_t net_drivers_table[] = {
    // Driver Intel (e1000)
    { 0x8086, 0x100e, "e1000.ko" }, // QEMU / VirtualBox PRO/1000 MT
    { 0x8086, 0x100f, "e1000.ko" }, // VMware PRO/1000 MT Server
    { 0x8086, 0x1004, "e1000.ko" }, // PRO/1000 MT Server (82543GC)
    { 0x8086, 0x153A, "e1000.ko" }, // I217-LM
    { 0x8086, 0x10EA, "e1000.ko" }, // 82577LM

    // Driver Realtek / D-Link (rtl8169)
    { 0x10EC, 0x8169, "rtl8169.ko" }, // Realtek PCI GbE Family Controller
    { 0x10EC, 0x8168, "rtl8169.ko" }, // Realtek PCIe GbE Family Controller
    { 0x10EC, 0x8161, "rtl8169.ko" }, // Realtek PCIe GbE Family Controller
    { 0x10EC, 0x8136, "rtl8169.ko" }, // Realtek PCIe FE Family Controller
    { 0x1186, 0x4300, "rtl8169.ko" }, // D-Link/Realtek PCI GbE Family Controller

    // Driver AMD PCnet (pcnet)
    { 0x1022, 0x2000, "pcnet.ko" }, // Am79C970 / PCnet32 LANCE
    { 0x1022, 0x2001, "pcnet.ko" }, // Am79C978 PCnet Home
};

#define NET_DRIVERS_COUNT 11

void kmod_net_init(const char* root) {
    char path[256];
    bool found = false;
    pci_device_t *current = NULL;

    while ((current = pci_find_by_class(PCI_CLASS_NETWORK, current)) != NULL)
    {
        found = false;

        for (size_t i = 0; i < NET_DRIVERS_COUNT; i++)
        {
            if (current->vendor_id == net_drivers_table[i].vendor_id &&
                current->device_id == net_drivers_table[i].device_id)
            {
                ksprintf(path, "%s/%s", root, net_drivers_table[i].driver_name);
                found = true;
                break; 
            }
        }

        if (found)
        {
            if (kmod_load_by_name(path, current) != 0)
            {
                kprintf("[kmod]: Erro critico: Falha ao carregar o modulo '%s'.\n", path);
            }
            else
            {
                kprintf("[kmod]: Modulo '%s' carregado com sucesso!\n", path);
            }
        }
        else
        {
            kprintf("Unknown ethernet device\n");
            kprintf("Vendor ID: %x, Device ID: %x\n", current->vendor_id, current->device_id);
            for(;;);
        }
    }
}