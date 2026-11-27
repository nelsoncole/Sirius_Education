#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define XHCI_INPUT_ADD_SLOT       (1u << 0)
#define XHCI_INPUT_ADD_EP0        (1u << 1)
#define XHCI_SLOT_SPEED_SHIFT     20
#define XHCI_SLOT_CONTEXT_SHIFT  27
#define XHCI_SLOT_ROOT_PORT_SHIFT 16
#define XHCI_EP_CERR_SHIFT       1
#define XHCI_EP_TYPE_SHIFT       3
#define XHCI_EP_MAX_PACKET_SHIFT 16
#define XHCI_EP_TYPE_CONTROL     4u
#define XHCI_EP_CERR             3u
#define XHCI_EP_DCS              1ULL
#define XHCI_EP_AVG_TRB_LENGTH   8u

static inline void xhci_context_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static int xhci_context_dma_valid(xhci_controller_t *x,
                                  const xhci_dma_t *dma,
                                  size_t minimum_size)
{
    if (!x || !dma || !dma->virt || !dma->phys)
        return -1;

    if (dma->size < minimum_size)
        return -2;

    if (((uintptr_t)dma->virt & 0x3Fu) || (dma->phys & 0x3Fu))
        return -3;

    if (!x->caps.address_64bit && (dma->phys >> 32))
        return -4;

    return 0;
}

static volatile uint32_t *xhci_input_context_pointer(xhci_controller_t *x,
                                                      xhci_device_t *device,
                                                      uint32_t index)
{
    if (!x || !device || !device->context.initialized || index > 31)
        return NULL;

    if (x->caps.context_size != 32 && x->caps.context_size != 64)
        return NULL;

    size_t offset = (size_t)index * x->caps.context_size;

    if (offset > device->context.input_dma.size ||
        device->context.input_dma.size - offset < x->caps.context_size)
        return NULL;

    return (volatile uint32_t *)((uintptr_t)device->context.input_dma.virt +
                                 offset);
}

int xhci_context_init(xhci_controller_t *x)
{
    if (!x || x->caps.max_slots == 0)
        return -1;

    size_t required_size =
        ((size_t)x->caps.max_slots + 1u) * sizeof(uint64_t);

    if (xhci_context_dma_valid(x, &x->dcbaa, required_size) != 0)
        return -2;

    memset(x->dcbaa.virt, 0, x->dcbaa.size);
    xhci_context_barrier();

    return 0;
}

int xhci_context_alloc(xhci_controller_t *x, xhci_device_t *device,
                       uint8_t slot_id)
{
    if (!x || !device || !slot_id || slot_id > x->caps.max_slots)
        return -1;

    if (device->context.initialized)
        return -2;

    size_t dcbaa_required =
        ((size_t)x->caps.max_slots + 1u) * sizeof(uint64_t);

    if (xhci_context_dma_valid(x, &x->dcbaa, dcbaa_required) != 0)
        return -3;

    volatile uint64_t *dcbaa = (volatile uint64_t *)x->dcbaa.virt;

    if (dcbaa[slot_id] != 0)
        return -4;

    if (x->caps.context_size != 32 && x->caps.context_size != 64)
        return -5;

    if (xhci_dma_alloc(&device->context.input_dma, PAGE_SIZE) != 0)
        return -6;

    if (xhci_dma_alloc(&device->context.device_dma, PAGE_SIZE) != 0)
        return -7;

    if (xhci_context_dma_valid(x, &device->context.input_dma, PAGE_SIZE) != 0 ||
        xhci_context_dma_valid(x, &device->context.device_dma, PAGE_SIZE) != 0)
        return -8;

    memset(device->context.input_dma.virt, 0,
           device->context.input_dma.size);
    memset(device->context.device_dma.virt, 0,
           device->context.device_dma.size);

    device->context.slot_id = slot_id;
    device->context.initialized = 1;

    dcbaa[slot_id] = device->context.device_dma.phys;
    xhci_context_barrier();

    return 0;
}

int xhci_context_prepare_address(xhci_controller_t *x,
                                 xhci_device_t *device,
                                 uint8_t port_id,
                                 uint8_t speed,
                                 uint16_t ep0_max_packet,
                                 uint64_t ep0_ring_phys)
{
    if (!x || !device || !device->context.initialized)
        return -1;

    if (!port_id || port_id > x->caps.max_ports || !speed || speed > 15)
        return -2;

    if (!ep0_max_packet || !ep0_ring_phys || (ep0_ring_phys & 0xFu))
        return -3;

    if (!x->caps.address_64bit && (ep0_ring_phys >> 32))
        return -4;

    if (x->caps.context_size != 32 && x->caps.context_size != 64)
        return -5;

    if (device->context.input_dma.size < (size_t)x->caps.context_size * 3u)
        return -6;

    volatile uint32_t *control =
        xhci_input_context_pointer(x, device, 0);
    volatile uint32_t *slot =
        xhci_input_context_pointer(x, device, 1);
    volatile uint32_t *ep0 =
        xhci_input_context_pointer(x, device, 2);

    if (!control || !slot || !ep0)
        return -7;

    memset(device->context.input_dma.virt, 0,
           device->context.input_dma.size);

    control[1] = XHCI_INPUT_ADD_SLOT | XHCI_INPUT_ADD_EP0;

    slot[0] = ((uint32_t)speed << XHCI_SLOT_SPEED_SHIFT) |
              (1u << XHCI_SLOT_CONTEXT_SHIFT);
    slot[1] = (uint32_t)port_id << XHCI_SLOT_ROOT_PORT_SHIFT;

    ep0[0] = 0;
    ep0[1] = (XHCI_EP_CERR << XHCI_EP_CERR_SHIFT) |
             (XHCI_EP_TYPE_CONTROL << XHCI_EP_TYPE_SHIFT) |
             ((uint32_t)ep0_max_packet << XHCI_EP_MAX_PACKET_SHIFT);
    ep0[2] = ((uint32_t)ep0_ring_phys & ~0xFu) |
             (uint32_t)XHCI_EP_DCS;
    ep0[3] = (uint32_t)(ep0_ring_phys >> 32);
    ep0[4] = XHCI_EP_AVG_TRB_LENGTH;

    device->port_id = port_id;
    device->speed = speed;
    device->max_packet_size = ep0_max_packet;

    xhci_context_barrier();

    return 0;
}

uint64_t xhci_context_get_input_phys(xhci_device_t *device)
{
    if (!device || !device->context.initialized)
        return 0;

    return device->context.input_dma.phys;
}

void xhci_context_destroy(xhci_controller_t *x, xhci_device_t *device)
{
    if (!x || !device || !device->context.initialized)
        return;

    uint8_t slot_id = device->context.slot_id;

    if (slot_id && slot_id <= x->caps.max_slots && x->dcbaa.virt)
    {
        volatile uint64_t *dcbaa = (volatile uint64_t *)x->dcbaa.virt;
        dcbaa[slot_id] = 0;
    }

    xhci_context_barrier();

    xhci_dma_free(&device->context.input_dma);
    xhci_dma_free(&device->context.device_dma);

    memset(&device->context, 0, sizeof(device->context));
}