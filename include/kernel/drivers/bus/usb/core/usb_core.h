/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usb_core.h
 *    Description: Tipos USB, descritores, transporte e interfaces de
 *                 enumeração e hubs para o subsistema USB do Sirius.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *        License: MIT
 * ============================================================================
 */
#ifndef _USB_CORE_H_
#define _USB_CORE_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

#define USB_DIR_IN              0x80
#define USB_DIR_OUT             0x00
#define USB_TYPE_STANDARD       0x00
#define USB_TYPE_CLASS          0x20
#define USB_RECIP_DEVICE        0x00
#define USB_RECIP_INTERFACE     0x01
#define USB_RECIP_ENDPOINT      0x02
#define USB_RECIP_OTHER         0x03
#define USB_REQ_GET_DESCRIPTOR  6
#define USB_REQ_SET_ADDRESS     5
#define USB_REQ_SET_CONFIGURATION 9
#define USB_REQ_GET_STATUS      0
#define USB_REQ_CLEAR_FEATURE   1
#define USB_REQ_SET_FEATURE     3
#define USB_DESC_DEVICE         1
#define USB_DESC_CONFIGURATION  2
#define USB_DESC_INTERFACE      4
#define USB_DESC_ENDPOINT       5
#define USB_DESC_HUB            0x29
#define USB_CLASS_HUB           9
#define USB_CLASS_MASS_STORAGE  8
#define USB_FEATURE_PORT_RESET  4
#define USB_FEATURE_PORT_POWER  8
#define USB_FEATURE_C_PORT_CONNECTION 16
#define USB_FEATURE_C_PORT_RESET 20
#define USB_HUB_PORT_STATUS     0
#define USB_HUB_PORT_CHANGE     2
#define USB_MAX_CONFIG_SIZE     1024
#define USB_MAX_ENDPOINTS       16
#define USB_MAX_DEVICES         64

typedef struct __attribute__((packed)) {
    uint8_t length, descriptor_type;
    uint16_t usb_version;
    uint8_t device_class, device_subclass, device_protocol, max_packet0;
    uint16_t vendor_id, product_id, device_version;
    uint8_t manufacturer, product, serial_number, configurations;
} usb_device_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t length, descriptor_type;
    uint16_t total_length;
    uint8_t interfaces, configuration_value, configuration_string;
    uint8_t attributes, max_power;
} usb_configuration_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t length, descriptor_type, interface_number, alternate_setting;
    uint8_t endpoints, interface_class, interface_subclass, interface_protocol;
    uint8_t interface_string;
} usb_interface_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t length, descriptor_type, endpoint_address, attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} usb_endpoint_descriptor_t;

typedef struct {
    uint8_t address, slot_id, port_number, speed;
    uint8_t configuration, interface_class, interface_subclass, interface_protocol;
    uint8_t endpoint_count, endpoint_in[USB_MAX_ENDPOINTS], endpoint_out[USB_MAX_ENDPOINTS];
    uint16_t endpoint_packet[USB_MAX_ENDPOINTS];
    uint8_t is_hub, connected, configured;
    usb_device_descriptor_t descriptor;
    void *host_data;
} usb_device_t;

/*
 * O host xHCI deve fornecer estas operações usando Control/Bulk/Interrupt
 * Transfer Rings. As funções não devem retornar antes da conclusão do TRB
 * (ou de timeout/erro), salvo quando a operação for explicitamente assíncrona.
 */
typedef struct {
    int (*control)(void *host, usb_device_t *dev, uint8_t request_type,
                   uint8_t request, uint16_t value, uint16_t index,
                   void *data, uint16_t length, uint32_t timeout_ms);
    int (*bulk)(void *host, usb_device_t *dev, uint8_t endpoint,
                void *data, uint32_t length, uint32_t timeout_ms);
    int (*interrupt)(void *host, usb_device_t *dev, uint8_t endpoint,
                     void *data, uint32_t length, uint32_t timeout_ms);
    int (*enable_slot)(void *host, uint8_t *slot_id);
    int (*address_device)(void *host, usb_device_t *dev);
    int (*configure_endpoints)(void *host, usb_device_t *dev,
                               const void *config, uint16_t config_length);
    int (*reset_port)(void *host, uint8_t root_port);
    void (*device_added)(usb_device_t *dev);
    void (*device_removed)(usb_device_t *dev);
} usb_host_ops_t;

typedef struct {
    void *host;
    const usb_host_ops_t *ops;
    usb_device_t devices[USB_MAX_DEVICES];
    uint32_t device_count;
} usb_bus_t;

typedef struct __attribute__((packed)) {
    uint8_t length, descriptor_type, ports, characteristics[2];
    uint8_t power_good, current;
} usb_hub_descriptor_t;

typedef struct __attribute__((packed)) {
    uint16_t status;
    uint16_t change;
} usb_port_status_t;

int usb_enumerate_device(usb_bus_t *bus, uint8_t root_port, uint8_t speed,
                         usb_device_t **out_device);
int usb_hub_initialize(usb_bus_t *bus, usb_device_t *hub);
int usb_hub_scan_ports(usb_bus_t *bus, usb_device_t *hub);
int usb_hub_enumerate_port(usb_bus_t *bus, usb_device_t *hub,
                           uint8_t port_number);
#endif
