
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: xhci.h
 *    Description: Estruturas principais e interfaces do controlador xHCI.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _XHCI_H_
#define _XHCI_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

#include <kernel/drivers/bus/pci/pci.h>
#include <kernel/kernel/core/spinlock.h>

#include <kernel/drivers/bus/usb/host/xhci/xhci_dma.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>

/* ============================================================================
 * Configuration
 * ========================================================================== */

#define XHCI_MAX_SLOTS             256
#define XHCI_MAX_PORTS             255
#define XHCI_MAX_ENDPOINTS         31
#define XHCI_MAX_INTERRUPTERS      2048

#define XHCI_MAX_DEVICES           256

#define XHCI_TRANSFER_TIMEOUT      10000000

/* ============================================================================
 * Controller Capabilities
 * ========================================================================== */

typedef struct
{
    uint16_t version;

    uint8_t cap_length;
    uint8_t max_slots;
    uint8_t max_ports;

    uint16_t max_interrupters;

    uint32_t dboff;
    uint32_t rtsoff;

    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;

    uint32_t hccparams1;
    uint32_t hccparams2;

    uint32_t pagesize;

    uint16_t scratchpad_count;

    uint8_t context_size;
    uint8_t address_64bit;
} xhci_caps_t;

/* ============================================================================
 * Generic Ring
 * ========================================================================== */

typedef struct
{
    xhci_dma_t dma;

    volatile xhci_trb_t *trbs;

    uint16_t enqueue;

    uint8_t cycle;

    uint8_t initialized;
} xhci_ring_t;

/* ============================================================================
 * Event Ring
 * ========================================================================== */

typedef struct
{
    xhci_dma_t ring_dma;
    xhci_dma_t erst_dma;

    volatile xhci_trb_t *trbs;

    uint16_t dequeue;

    uint8_t cycle;

    uint16_t size;

    uint8_t initialized;
} xhci_event_ring_t;

/* ============================================================================
 * Device Context Management
 * ========================================================================== */

typedef struct
{
    xhci_dma_t dma;

    volatile uint32_t *entries;

    uint16_t max_slots;

    uint8_t initialized;
} xhci_dcbaa_t;

typedef struct
{
    xhci_dma_t input_dma;
    xhci_dma_t device_dma;

    uint8_t slot_id;

    uint8_t initialized;
} xhci_device_context_t;

/* ============================================================================
 * Scratchpad Management
 * ========================================================================== */

typedef struct
{
    xhci_dma_t array_dma;

    xhci_dma_t *buffers;

    uint16_t count;

    uint8_t initialized;
} xhci_scratchpad_t;

/* ============================================================================
 * Device Information
 * ========================================================================== */

typedef struct
{
    uint8_t connected;
    uint8_t enabled;
    uint8_t addressed;

    uint8_t slot_id;
    uint8_t port_id;
    uint8_t speed;

    uint8_t root_hub_port;

    uint16_t vendor_id;
    uint16_t product_id;

    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;

    uint8_t configuration_value;

    uint8_t endpoint_count;

    uint16_t max_packet_size;

    xhci_ring_t ep0_ring;

    xhci_device_context_t context;
} xhci_device_t;

/* ============================================================================
 * Command Synchronization
 * ========================================================================== */

typedef struct
{
    volatile uint32_t busy;
    volatile uint32_t completed;
    volatile uint32_t faulted;

    volatile uint64_t expected_trb;
    volatile uint64_t completed_trb;

    volatile uint32_t residual;

    volatile uint8_t completion_code;
    volatile uint8_t slot_id;
} xhci_command_state_t;


/* ============================================================================
 * Device Management
 * Estrutura do pedido SETUP
 * ========================================================================== */

typedef struct __attribute__((packed))
{
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} xhci_setup_packet_t;

/* ============================================================================
 * Controller Structure
 * ========================================================================== */

typedef struct
{
    pci_device_t *pci;

    uintptr_t mmio_phys;

    volatile uint8_t *mmio;
    volatile uint8_t *op;
    volatile uint8_t *runtime;
    volatile uint8_t *doorbells;

    xhci_caps_t caps;

    xhci_dma_t dcbaa;

    xhci_ring_t command_ring;

    xhci_event_ring_t event_ring;

    xhci_scratchpad_t scratchpads;

    xhci_command_state_t command;

    volatile uint32_t transfer_busy;
    volatile uint32_t transfer_completed;
    volatile uint32_t transfer_faulted;

    volatile uint64_t transfer_expected_trb;
    volatile uint64_t transfer_completed_trb;

    volatile uint32_t transfer_residual;

    volatile uint8_t transfer_completion_code;
    volatile uint8_t transfer_slot_id;
    volatile uint8_t transfer_endpoint_id;

    /* Bitset: cada bit representa uma porta com evento pendente. */
    volatile uint32_t port_change_pending[8];

    xhci_device_t devices[XHCI_MAX_DEVICES];

    uint16_t device_count;

    uint8_t initialized;

    uint8_t running;

    spinlock_t lock;

} xhci_controller_t;


/* ============================================================================
 * Global Controller
 * ========================================================================== */

extern xhci_controller_t g_xhci_controller;

/* ============================================================================
 * Controller Initialization
 * ========================================================================== */

int xhci_init(pci_device_t *dev);

/* ============================================================================
 * DMA
 * ========================================================================== */

int xhci_dma_alloc(xhci_dma_t *dma, size_t size);

/* ============================================================================
 * Command Ring
 * ========================================================================== */

int xhci_ring_init(xhci_controller_t *xhci);

int xhci_command_submit(
    xhci_controller_t *xhci,
    const xhci_trb_t *command,
    uint32_t timeout,
    uint8_t *slot_id,
    uint8_t *completion_code
);

/* ============================================================================
 * Event Ring
 * ========================================================================== */

int xhci_event_ring_init(xhci_controller_t *xhci);

void xhci_process_events(xhci_controller_t *xhci);

void xhci_interrupt_handler(void);

/* ============================================================================
 * Ports
 * ========================================================================== */

void xhci_scan_ports(xhci_controller_t *xhci);
void xhci_process_port_changes(xhci_controller_t *xhci);

int xhci_reset_port(
    xhci_controller_t *xhci,
    uint8_t port_id
);

/* ============================================================================
 * Contexts
 * ========================================================================== */
int xhci_context_init(xhci_controller_t *x);

int xhci_context_alloc(
    xhci_controller_t *x,
    xhci_device_t *device,
    uint8_t slot_id
);

int xhci_context_prepare_address(
    xhci_controller_t *x,
    xhci_device_t *device,
    uint8_t port_id,
    uint8_t speed,
    uint16_t ep0_max_packet,
    uint64_t ep0_ring_phys
);

uint64_t xhci_context_get_input_phys(xhci_device_t *device);

void xhci_context_destroy(
    xhci_controller_t *x,
    xhci_device_t *device
);


/* ============================================================================
 * Commands
 * ========================================================================== */

int xhci_enable_slot(
    xhci_controller_t *xhci,
    uint8_t *slot_id
);

int xhci_disable_slot(
    xhci_controller_t *xhci,
    uint8_t slot_id
);

int xhci_address_device(
    xhci_controller_t *xhci,
    uint8_t slot_id,
    uint64_t input_context,
    uint8_t block_set_address
);

int xhci_configure_endpoint(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint64_t input_context,
    uint8_t deconfigure
);

int xhci_evaluate_context(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint64_t input_context
);

/* ============================================================================
 * Transfers
 * ========================================================================== */

int xhci_transfer_ring_init(
    xhci_controller_t *x,
    xhci_ring_t *ring
);

int xhci_control_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    const xhci_setup_packet_t *setup,
    xhci_dma_t *buffer,
    uint32_t length,
    uint8_t direction_in,
    uint32_t *actual_length
);

int xhci_bulk_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    xhci_ring_t *ring,
    uint8_t endpoint_id,
    xhci_dma_t *buffer,
    uint32_t length,
    uint32_t *actual_length
);

int xhci_interrupt_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    xhci_ring_t *ring,
    uint8_t endpoint_id,
    xhci_dma_t *buffer,
    uint32_t length,
    uint32_t *actual_length
);

/* ============================================================================
 * Device Management
 * ========================================================================== */
int xhci_enumerate_device(
    xhci_controller_t *x,
    uint8_t port_id,
    uint8_t speed
);

void xhci_device_disconnect(
    xhci_controller_t *x,
    uint8_t slot_id
);



#define XHCI_TRB_TYPE_NOOP_COMMAND 23
int xhci_test_dma(xhci_controller_t *x);

#endif
