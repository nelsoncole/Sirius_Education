/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usb_enum.c
 *    Description: Enumeração USB: Enable Slot, Device Descriptor, Address,
 *                 Configuration Descriptor e configuração dos endpoints.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *        License: MIT
 * ============================================================================
 */
#include <kernel/drivers/bus/usb/core/usb_core.h>
#include <kernel/klib.h>

static uint16_t usb_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int usb_read_descriptors(usb_bus_t *bus, usb_device_t *dev,
                                uint8_t *config, uint16_t *config_len)
{
    int rc = bus->ops->control(bus->host, dev,
        USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_DEVICE << 8), 0,
        &dev->descriptor, 8, 1000);
    if (rc < 0) return rc;

    uint16_t packet0 = dev->descriptor.max_packet0;
    if (dev->speed == 4) packet0 = 512;
    else if (dev->speed == 3) packet0 = 64;
    else if (dev->speed == 2) packet0 = 8;
    if (!packet0) return -1;

    rc = bus->ops->control(bus->host, dev,
        USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_DEVICE << 8), 0,
        &dev->descriptor, sizeof(dev->descriptor), 1000);
    if (rc < 0 || dev->descriptor.length < sizeof(usb_device_descriptor_t))
        return -2;

    uint8_t head[9];
    rc = bus->ops->control(bus->host, dev,
        USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0,
        head, sizeof(head), 1000);
    if (rc < 0) return -3;

    uint16_t total = usb_le16(&head[2]);
    if (total < sizeof(usb_configuration_descriptor_t) ||
        total > USB_MAX_CONFIG_SIZE) return -4;

    rc = bus->ops->control(bus->host, dev,
        USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
        USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0,
        config, total, 2000);
    if (rc < 0) return -5;
    *config_len = total;
    return 0;
}

static void usb_parse_configuration(usb_device_t *dev,
                                    const uint8_t *buf, uint16_t len)
{
    uint16_t off = 0;
    dev->endpoint_count = 0;
    while (off + 2 <= len) {
        uint8_t dlen = buf[off], dtype = buf[off + 1];
        if (dlen < 2 || off + dlen > len) break;
        if (dtype == USB_DESC_INTERFACE && dlen >= 9) {
            dev->interface_class = buf[off + 5];
            dev->interface_subclass = buf[off + 6];
            dev->interface_protocol = buf[off + 7];
        } else if (dtype == USB_DESC_ENDPOINT && dlen >= 7 &&
                   dev->endpoint_count < USB_MAX_ENDPOINTS) {
            uint8_t n = dev->endpoint_count++;
            dev->endpoint_in[n] = buf[off + 2];
            dev->endpoint_out[n] = buf[off + 2];
            dev->endpoint_packet[n] = (uint16_t)buf[off + 4] |
                                      ((uint16_t)buf[off + 5] << 8);
        }
        off += dlen;
    }
}

int usb_enumerate_device(usb_bus_t *bus, uint8_t root_port, uint8_t speed,
                         usb_device_t **out_device)
{
    if (!bus || !bus->ops || !bus->ops->control ||
        !bus->ops->enable_slot || !bus->ops->address_device ||
        !bus->ops->configure_endpoints ||
        bus->device_count >= USB_MAX_DEVICES) return -1;

    if (bus->ops->reset_port && bus->ops->reset_port(bus->host, root_port) < 0)
        return -2;

    usb_device_t *dev = &bus->devices[bus->device_count];
    memset(dev, 0, sizeof(*dev));
    dev->port_number = root_port;
    dev->speed = speed;
    dev->connected = 1;

    if (bus->ops->enable_slot(bus->host, &dev->slot_id) < 0 ||
        !dev->slot_id) return -3;

    if (bus->ops->address_device(bus->host, dev) < 0) return -4;

    uint8_t config[USB_MAX_CONFIG_SIZE];
    uint16_t config_len = 0;
    if (usb_read_descriptors(bus, dev, config, &config_len) < 0) return -5;

    usb_configuration_descriptor_t *cd =
        (usb_configuration_descriptor_t *)config;
    if (cd->descriptor_type != USB_DESC_CONFIGURATION ||
        cd->length < sizeof(*cd)) return -6;

    usb_parse_configuration(dev, config, config_len);
    if (bus->ops->configure_endpoints(bus->host, dev,
                                      config, config_len) < 0) return -7;

    if (bus->ops->control(bus->host, dev,
        USB_TYPE_STANDARD | USB_RECIP_DEVICE, USB_REQ_SET_CONFIGURATION,
        cd->configuration_value, 0, NULL, 0, 1000) < 0) return -8;

    dev->configuration = cd->configuration_value;
    dev->configured = 1;
    dev->is_hub = (dev->interface_class == USB_CLASS_HUB);
    bus->device_count++;
    if (out_device) *out_device = dev;
    if (bus->ops->device_added) bus->ops->device_added(dev);

    if (dev->is_hub) return usb_hub_initialize(bus, dev);
    return 0;
}
