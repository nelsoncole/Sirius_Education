/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usb_hub.c
 *    Description: Suporte de classe Hub: descritor, alimentação, leitura de
 *                 estado, reset e enumeração dos dispositivos downstream.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *        License: MIT
 * ============================================================================
 */
#include <kernel/drivers/bus/usb/core/usb_core.h>
#include <kernel/klib.h>

#define USB_PORT_CONNECTION  (1u << 0)
#define USB_PORT_ENABLE      (1u << 1)
#define USB_PORT_RESET       (1u << 4)
#define USB_PORT_LOW_SPEED   (1u << 9)
#define USB_PORT_HIGH_SPEED  (1u << 10)
#define USB_PORT_POWER       (1u << 8)

static int hub_get_port_status(usb_bus_t *bus, usb_device_t *hub,
                               uint8_t port, usb_port_status_t *status)
{
    return bus->ops->control(bus->host, hub,
        USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_OTHER,
        USB_HUB_PORT_STATUS, 0, port, status, sizeof(*status), 1000);
}

int usb_hub_initialize(usb_bus_t *bus, usb_device_t *hub)
{
    if (!bus || !hub || !hub->is_hub || !bus->ops ||
        !bus->ops->control) return -1;

    usb_hub_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    int rc = bus->ops->control(bus->host, hub,
        USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_HUB << 8), 0,
        &desc, sizeof(desc), 1000);
    if (rc < 0 || desc.descriptor_type != USB_DESC_HUB ||
        desc.length < sizeof(desc) || !desc.ports) return -2;

    for (uint8_t p = 1; p <= desc.ports; p++) {
        bus->ops->control(bus->host, hub,
            USB_TYPE_CLASS | USB_RECIP_OTHER, USB_REQ_SET_FEATURE,
            USB_FEATURE_PORT_POWER, p, NULL, 0, 1000);
    }
    return 0;
}

int usb_hub_enumerate_port(usb_bus_t *bus, usb_device_t *hub,
                           uint8_t port_number)
{
    if (!bus || !hub || !port_number) return -1;
    usb_port_status_t status;
    if (hub_get_port_status(bus, hub, port_number, &status) < 0)
        return -2;
    if (!(status.status & USB_PORT_CONNECTION)) return 0;

    uint8_t speed = 1;
    if (status.status & USB_PORT_HIGH_SPEED) speed = 3;
    else if (status.status & USB_PORT_LOW_SPEED) speed = 2;

    if (bus->ops->control(bus->host, hub,
        USB_TYPE_CLASS | USB_RECIP_OTHER, USB_REQ_SET_FEATURE,
        USB_FEATURE_PORT_RESET, port_number, NULL, 0, 1000) < 0)
        return -3;

    if (bus->ops->control(bus->host, hub,
        USB_TYPE_CLASS | USB_RECIP_OTHER, USB_REQ_CLEAR_FEATURE,
        USB_FEATURE_C_PORT_RESET, port_number, NULL, 0, 1000) < 0)
        return -4;

    /*
     * O hub externo já encaminhou o reset. A camada host deve mapear a porta
     * downstream para o caminho/Route String xHCI e preencher slot/contextos.
     * Até essa integração existir, não se pode tratar port_number como root port.
     */
    (void)speed;
    return -5;
}

int usb_hub_scan_ports(usb_bus_t *bus, usb_device_t *hub)
{
    if (!bus || !hub || !hub->is_hub) return -1;
    usb_hub_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    if (bus->ops->control(bus->host, hub,
        USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_HUB << 8), 0,
        &desc, sizeof(desc), 1000) < 0) return -2;

    int found = 0;
    for (uint8_t p = 1; p <= desc.ports; p++) {
        usb_port_status_t st;
        if (hub_get_port_status(bus, hub, p, &st) == 0 &&
            (st.status & USB_PORT_CONNECTION)) {
            int rc = usb_hub_enumerate_port(bus, hub, p);
            if (rc == 0) found++;
        }
    }
    return found;
}
