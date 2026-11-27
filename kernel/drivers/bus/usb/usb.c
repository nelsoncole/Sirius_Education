/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usb.c
 *    Description: Implementacao do subsistema USB. Responsavel por identificar
 *                 os controladores USB registados no barramento PCI e
 *                 inicializar os controladores xHCI suportados.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 26/11/2026
 *
 *        License: MIT
 * ============================================================================
 */
#include <kernel/drivers/bus/usb/usb.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/klib.h>

int usb_init(pci_device_t *dev)
{
    if (!dev)
        return -1;

    switch (dev->prog_if)
    {
        case PCI_USB_XHCI:
            kprintf("[USB] Controlador xHCI encontrado.\n");

            if (xhci_init(dev) != 0)
            {
                kprintf("[USB] Falha ao inicializar xHCI.\n");
                return -1;
            }

            kprintf("[USB] xHCI inicializado com sucesso.\n");
            return 0;

        default:
            kprintf("[USB] Interface USB nao suportada: 0x%x\n",
                    dev->prog_if);
            return -1;
    }
}

void usb_driver_init(void)
{
    pci_load_devices_by_class(
        PCI_CLASS_SERIAL_BUS,
        PCI_SUBCLASS_USB,
        usb_init
    );
}