/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usb.h
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

#ifndef USB_H
#define USB_H

#include <kernel/drivers/bus/pci/pci.h>

void usb_driver_init(void);
int usb_init(pci_device_t *dev);

#endif