#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>
#include <kernel/arch/x86_64/kapi/timer.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define USB_DESC_DEVICE             1u
#define USB_REQ_GET_DESCRIPTOR      6u
#define USB_REQUEST_DEVICE_IN       0x80u

#define USB_EP0_LOW_SPEED           8u
#define USB_EP0_FULL_SPEED          8u
#define USB_EP0_HIGH_SPEED          64u
#define USB_EP0_SUPER_SPEED         512u

#define USB_DEVICE_DESC_SIZE        18u

typedef struct __attribute__((packed))
{
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t serial_number;
    uint8_t num_configurations;
} usb_device_descriptor_t;

_Static_assert(sizeof(usb_device_descriptor_t) == USB_DEVICE_DESC_SIZE,
               "USB device descriptor must be 18 bytes");


static volatile uint32_t *xhci_device_portsc_ptr(
    xhci_controller_t *x,
    uint8_t port_id
)
{
    if (!x || !x->op || !port_id || port_id > x->caps.max_ports)
        return NULL;

    return (volatile uint32_t *)(
        x->op + XHCI_PORTSC_BASE +
        ((uint32_t)port_id - 1u) * XHCI_PORT_STRIDE
    );
}

static void xhci_device_debug_port(
    xhci_controller_t *x,
    uint8_t port_id,
    uint8_t expected_speed,
    const char *label
)
{
    volatile uint32_t *reg =
        xhci_device_portsc_ptr(x, port_id);

    if (!reg)
        return;

    uint32_t value = *reg;
    uint8_t port_speed = (uint8_t)((value >> 10) & 0x0Fu);

    kprintf(
        "[xHCI PORT DEBUG] %s port=%u PORTSC=%08x "
        "CCS=%u PED=%u PR=%u PP=%u SPEED=%u expected=%u "
        "CSC=%u PEC=%u WRC=%u OCC=%u PRC=%u PLC=%u CEC=%u\n",
        label,
        port_id,
        value,
        value & 1u,
        (value >> 1) & 1u,
        (value >> 4) & 1u,
        (value >> 9) & 1u,
        port_speed,
        expected_speed,
        (value >> 17) & 1u,
        (value >> 18) & 1u,
        (value >> 19) & 1u,
        (value >> 20) & 1u,
        (value >> 21) & 1u,
        (value >> 22) & 1u,
        (value >> 23) & 1u
    );

    if (port_speed != expected_speed)
    {
        kprintf(
            "[xHCI PORT DEBUG] WARNING: speed recebido (%u) "
            "difere do PORTSC (%u)\n",
            expected_speed,
            port_speed
        );
    }
}


static uint16_t xhci_device_ep0_packet_size(uint8_t speed)
{
    switch (speed)
    {
        case 1:
            return USB_EP0_FULL_SPEED;

        case 2:
            return USB_EP0_LOW_SPEED;

        case 3:
            return USB_EP0_HIGH_SPEED;

        case 4:
            return USB_EP0_SUPER_SPEED;

        default:
            return 0;
    }
}


static int xhci_device_read_descriptor(
    xhci_controller_t *x,
    xhci_device_t *device,
    xhci_dma_t *buffer,
    uint16_t length,
    uint32_t *actual_length
)
{
    if (!x || !device || !buffer || !buffer->virt || !length)
        return -1;

    if (length > buffer->size)
        return -2;

    xhci_setup_packet_t setup;
    memset(&setup, 0, sizeof(setup));

    setup.request_type = USB_REQUEST_DEVICE_IN;
    setup.request = USB_REQ_GET_DESCRIPTOR;
    setup.value = (uint16_t)(USB_DESC_DEVICE << 8);
    setup.index = 0;
    setup.length = length;

    memset(buffer->virt, 0xAA, length);
    __asm__ volatile("mfence" ::: "memory");

    kprintf(
        "[xHCI DEBUG] GET_DESCRIPTOR: bmRequestType=%02x request=%02x "
        "value=%04x index=%04x length=%u\n",
        setup.request_type,
        setup.request,
        setup.value,
        setup.index,
        setup.length
    );

    kprintf(
        "[xHCI DEBUG] SETUP BYTES: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        ((uint8_t *)&setup)[0],
        ((uint8_t *)&setup)[1],
        ((uint8_t *)&setup)[2],
        ((uint8_t *)&setup)[3],
        ((uint8_t *)&setup)[4],
        ((uint8_t *)&setup)[5],
        ((uint8_t *)&setup)[6],
        ((uint8_t *)&setup)[7]
    );

    xhci_device_debug_port(
        x,
        device->port_id,
        device->speed,
        "BEFORE CONTROL TRANSFER"
    );

    kprintf(
        "[xHCI DMA TEST] BEFORE: virt=%p dma=%lx size=%u data=",
        buffer->virt,
        (unsigned long)buffer->phys,
        (unsigned)buffer->size
    );

    for (uint16_t i = 0; i < length && i < 18; i++)
        kprintf("%02x ", ((uint8_t *)buffer->virt)[i]);

    kprintf("\n");

    if (actual_length)
        *actual_length = 0;

    int result = xhci_control_transfer(
        x,
        device,
        &setup,
        buffer,
        length,
        1,
        actual_length
    );

    __asm__ volatile("mfence" ::: "memory");

    xhci_device_debug_port(
        x,
        device->port_id,
        device->speed,
        "AFTER CONTROL TRANSFER"
    );

    kprintf(
        "[xHCI DMA TEST] AFTER: result=%d actual=%u data=",
        result,
        actual_length ? *actual_length : 0
    );

    for (uint16_t i = 0; i < length && i < 18; i++)
        kprintf("%02x ", ((uint8_t *)buffer->virt)[i]);

    kprintf("\n");

    return result;
}


static int xhci_device_update_ep0_packet_size(
    xhci_controller_t *x,
    xhci_device_t *device,
    uint16_t packet_size
)
{
    if (!x || !device || !device->context.initialized)
        return -1;

    if (packet_size != 8 &&
        packet_size != 16 &&
        packet_size != 32 &&
        packet_size != 64)
        return -3;

    uint32_t context_size = x->caps.context_size;

    if (context_size != XHCI_CONTEXT_SIZE_32 &&
        context_size != XHCI_CONTEXT_SIZE_64)
        return -2;

    if (!device->context.input_dma.virt ||
        !device->context.device_dma.virt ||
        device->context.input_dma.size < 3u * context_size ||
        device->context.device_dma.size < 2u * context_size)
        return -2;

    volatile uint32_t *input_control =
        (volatile uint32_t *)device->context.input_dma.virt;

    volatile uint32_t *input_ep0 =
        (volatile uint32_t *)(
            (uintptr_t)device->context.input_dma.virt +
            (uintptr_t)2u * context_size
        );

    volatile uint32_t *device_ep0 =
        (volatile uint32_t *)(
            (uintptr_t)device->context.device_dma.virt +
            (uintptr_t)context_size
        );

    memset(
        device->context.input_dma.virt,
        0,
        device->context.input_dma.size
    );

    for (uint32_t i = 0;
         i < context_size / sizeof(uint32_t);
         i++)
    {
        input_ep0[i] = device_ep0[i];
    }

    input_control[1] = 1u << 1;

    input_ep0[1] =
        (input_ep0[1] & 0x0000FFFFu) |
        ((uint32_t)packet_size << 16);

    __asm__ volatile("mfence" ::: "memory");

    int result = xhci_evaluate_context(
        x,
        device->context.slot_id,
        device->context.input_dma.phys
    );

    if (result != 0)
    {
        kprintf(
            "[xHCI] Evaluate Context falhou: slot=%u resultado=%d\n",
            device->context.slot_id,
            result
        );

        return -3;
    }

    device->max_packet_size = packet_size;

    kprintf(
        "[xHCI] EP0 atualizado: slot=%u MPS=%u\n",
        device->context.slot_id,
        packet_size
    );

    return 0;
}


static int xhci_device_parse_descriptor(
    xhci_device_t *device,
    const usb_device_descriptor_t *descriptor
)
{
    if (!device || !descriptor)
        return -1;

    if (descriptor->length < USB_DEVICE_DESC_SIZE)
        return -2;

    if (descriptor->descriptor_type != USB_DESC_DEVICE)
        return -3;

    if (!descriptor->num_configurations)
        return -4;

    if (device->speed >= 4 && descriptor->max_packet_size != 9)
        return -5;

    device->vendor_id = descriptor->vendor_id;
    device->product_id = descriptor->product_id;

    device->device_class = descriptor->device_class;
    device->device_subclass = descriptor->device_subclass;
    device->device_protocol = descriptor->device_protocol;

    device->configuration_value = 0;

    return 0;
}


static void xhci_device_debug_context(
    xhci_controller_t *x,
    xhci_device_t *device,
    const char *label
)
{
    if (!x || !device ||
        !device->context.input_dma.virt ||
        !device->context.device_dma.virt)
        return;

    uint32_t context_size = x->caps.context_size;

    if (context_size != XHCI_CONTEXT_SIZE_32 &&
        context_size != XHCI_CONTEXT_SIZE_64)
        return;

    __asm__ volatile("mfence" ::: "memory");

    volatile uint32_t *input =
        (volatile uint32_t *)device->context.input_dma.virt;

    volatile uint32_t *input_slot =
        (volatile uint32_t *)(
            (uintptr_t)device->context.input_dma.virt + context_size
        );

    volatile uint32_t *input_ep0 =
        (volatile uint32_t *)(
            (uintptr_t)device->context.input_dma.virt + 2u * context_size
        );

    volatile uint32_t *device_context =
        (volatile uint32_t *)device->context.device_dma.virt;

    volatile uint32_t *device_slot = device_context;

    volatile uint32_t *device_ep0 =
        (volatile uint32_t *)(
            (uintptr_t)device->context.device_dma.virt + context_size
        );

    kprintf(
        "[xHCI DEBUG] %s context_size=%u slot=%u port=%u speed=%u\n",
        label,
        context_size,
        device->context.slot_id,
        device->port_id,
        device->speed
    );

    kprintf(
        "[xHCI DEBUG] INPUT CONTROL: %08x %08x %08x %08x\n",
        input[0], input[1], input[2], input[3]
    );

    kprintf(
        "[xHCI DEBUG] INPUT SLOT: %08x %08x %08x %08x\n",
        input_slot[0], input_slot[1], input_slot[2], input_slot[3]
    );

    kprintf(
        "[xHCI DEBUG] INPUT EP0: %08x %08x %08x %08x %08x %08x %08x %08x\n",
        input_ep0[0], input_ep0[1], input_ep0[2], input_ep0[3],
        input_ep0[4], input_ep0[5], input_ep0[6], input_ep0[7]
    );

    kprintf(
        "[xHCI DEBUG] DEVICE SLOT: %08x %08x %08x %08x\n",
        device_slot[0], device_slot[1], device_slot[2], device_slot[3]
    );

    kprintf(
        "[xHCI DEBUG] DEVICE EP0: %08x %08x %08x %08x %08x %08x %08x %08x\n",
        device_ep0[0], device_ep0[1], device_ep0[2], device_ep0[3],
        device_ep0[4], device_ep0[5], device_ep0[6], device_ep0[7]
    );

    kprintf(
        "[xHCI DEBUG] CONTEXT DMA: input=%lx device=%lx ep0_ring=%lx\n",
        (unsigned long)device->context.input_dma.phys,
        (unsigned long)device->context.device_dma.phys,
        (unsigned long)device->ep0_ring.dma.phys
    );
}


int xhci_enumerate_device(
    xhci_controller_t *x,
    uint8_t port_id,
    uint8_t speed
)
{
    if (!x || !x->running ||
        !port_id || port_id > x->caps.max_ports)
        return -1;

    const char *stage = "Enable Slot";

    kprintf(
        "[xHCI ENUM DEBUG] ENTER port=%u speed_arg=%u max_ports=%u "
        "context_size=%u address64=%u\n",
        port_id,
        speed,
        x->caps.max_ports,
        x->caps.context_size,
        x->caps.address_64bit
    );

    xhci_device_debug_port(
        x,
        port_id,
        speed,
        "ENUMERATION ENTRY"
    );

    uint16_t ep0_packet =
        xhci_device_ep0_packet_size(speed);

    if (!ep0_packet)
    {
        kprintf(
            "[xHCI] Velocidade USB desconhecida: porta=%u speed=%u\n",
            port_id,
            speed
        );

        return -2;
    }

    kprintf(
        "[xHCI ENUM DEBUG] speed=%u selected_EP0_MPS=%u\n",
        speed,
        ep0_packet
    );

    /* Tempo de recuperação pós-reset USB (tRSTRCY >= 10ms) */
    mdelay(20);

    xhci_device_debug_port(
        x,
        port_id,
        speed,
        "AFTER RECOVERY DELAY"
    );

    uint8_t slot_id = 0;

    int result = xhci_enable_slot(x, &slot_id);

    if (result != 0)
    {
        kprintf(
            "[xHCI] Enable Slot falhou: porta=%u resultado=%d\n",
            port_id,
            result
        );

        return -3;
    }

    if (!slot_id || slot_id > x->caps.max_slots)
    {
        kprintf(
            "[xHCI] Slot invalido recebido: %u\n",
            slot_id
        );

        if (slot_id)
            xhci_disable_slot(x, slot_id);

        return -4;
    }

    xhci_device_t *device = &x->devices[slot_id];

    memset(device, 0, sizeof(*device));

    device->slot_id = slot_id;
    device->port_id = port_id;
    device->root_hub_port = port_id;
    device->speed = speed;

    device->connected = 1;
    device->enabled = 1;
    device->max_packet_size = ep0_packet;

    xhci_dma_t descriptor_dma;
    memset(&descriptor_dma, 0, sizeof(descriptor_dma));

    stage = "Context Allocation";

    result = xhci_context_alloc(x, device, slot_id);

    if (result != 0)
    {
        kprintf(
            "[xHCI] Context Allocation falhou: resultado=%d\n",
            result
        );

        goto fail;
    }

    stage = "EP0 Transfer Ring";

    result = xhci_transfer_ring_init(x, &device->ep0_ring);

    if (result != 0)
    {
        kprintf(
            "[xHCI] EP0 Transfer Ring falhou: resultado=%d\n",
            result
        );

        goto fail;
    }

    stage = "Prepare Address Context";

    result = xhci_context_prepare_address(
        x,
        device,
        port_id,
        speed,
        ep0_packet,
        device->ep0_ring.dma.phys
    );

    if (result != 0)
    {
        kprintf(
            "[xHCI] Prepare Address Context falhou: resultado=%d\n",
            result
        );

        goto fail;
    }

    volatile uint32_t *portsc =
        xhci_device_portsc_ptr(x, port_id);

    if (!portsc)
    {
        kprintf("[xHCI] PORTSC pointer invalido para porta %u\n", port_id);
        goto fail;
    }

    xhci_device_debug_port(
        x,
        port_id,
        speed,
        "BEFORE ADDRESS DEVICE"
    );

    uint32_t context_size = x->caps.context_size;

    if (context_size != XHCI_CONTEXT_SIZE_32 &&
        context_size != XHCI_CONTEXT_SIZE_64)
    {
        kprintf("[xHCI] Context size invalido: %u\n", context_size);
        goto fail;
    }

    volatile uint32_t *slot =
        (volatile uint32_t *)(
            (uintptr_t)device->context.input_dma.virt + context_size
        );

    volatile uint32_t *ep0 =
        (volatile uint32_t *)(
            (uintptr_t)device->context.input_dma.virt + 2u * context_size
        );

    kprintf(
        "[xHCI DEBUG] PORTSC=%08x SPEED=%u CONTEXT_SIZE=%u\n",
        *portsc,
        speed,
        context_size
    );

    kprintf(
        "[xHCI DEBUG] INPUT: virt=%p phys=%lx\n",
        device->context.input_dma.virt,
        (unsigned long)xhci_context_get_input_phys(device)
    );

    kprintf(
        "[xHCI DEBUG] DEVICE: virt=%p phys=%lx\n",
        device->context.device_dma.virt,
        (unsigned long)device->context.device_dma.phys
    );

    kprintf(
        "[xHCI DEBUG] EP0 RING: virt=%p phys=%lx\n",
        device->ep0_ring.dma.virt,
        (unsigned long)device->ep0_ring.dma.phys
    );

    kprintf(
        "[xHCI DEBUG] EP0 MPS=%u\n",
        device->max_packet_size
    );

    kprintf(
        "[xHCI DEBUG] SLOT: %08x %08x %08x %08x\n",
        slot[0], slot[1], slot[2], slot[3]
    );

    kprintf(
        "[xHCI DEBUG] EP0: %08x %08x %08x %08x %08x\n",
        ep0[0], ep0[1], ep0[2], ep0[3], ep0[4]
    );

    stage = "Address Device (BSR=0)";

    kprintf(
        "[xHCI DEBUG] ADDRESS DEVICE: slot=%u input_context=%lx BSR=0\n",
        slot_id,
        (unsigned long)xhci_context_get_input_phys(device)
    );

    result = xhci_address_device(
        x,
        slot_id,
        xhci_context_get_input_phys(device),
        0
    );

    if (result != 0)
    {
        kprintf(
            "[xHCI] Address Device falhou: slot=%u resultado=%d\n",
            slot_id,
            result
        );

        goto fail;
    }

    __asm__ volatile("mfence" ::: "memory");

    xhci_device_debug_context(x, device, "AFTER ADDRESS DEVICE");

    xhci_device_debug_port(
        x,
        port_id,
        speed,
        "AFTER ADDRESS DEVICE"
    );

    device->addressed = 1;

    kprintf(
        "[xHCI] Address Device concluido para slot %u\n",
        slot_id
    );

    stage = "Descriptor DMA Allocation";
    result = xhci_dma_alloc(&descriptor_dma, PAGE_SIZE);
    if (result != 0)
    {
        kprintf(
            "[xHCI] Erro ao alocar DMA do Device Descriptor: %d\n",
            result);
        goto fail;
    }
    if (!descriptor_dma.virt ||
        descriptor_dma.size < USB_DEVICE_DESC_SIZE ||
        !descriptor_dma.phys ||
        (descriptor_dma.phys & 0x3u) ||
        (!x->caps.address_64bit && (descriptor_dma.phys >> 32)))
    {
        kprintf(
            "[xHCI] Buffer DMA do descritor invalido: virt=%p phys=%lx size=%u\n",
            descriptor_dma.virt,
            (unsigned long)descriptor_dma.phys,
            (unsigned)descriptor_dma.size);
        goto fail;
    }
    __asm__ volatile("mfence" ::: "memory");


    xhci_device_debug_context(x, device, "BEFORE GET_DESCRIPTOR");

    uint32_t actual = 0;
    stage = "GET_DESCRIPTOR 8 bytes";
    result = xhci_device_read_descriptor(
        x,
        device,
        &descriptor_dma,
        8,
        &actual);
    if (result != 0)
    {
        kprintf(
            "[xHCI] GET_DESCRIPTOR inicial falhou: result=%d actual=%u\n",
            result,
            actual);
        goto fail;
    }
    if (actual < 8)
    {
        kprintf(
            "[xHCI] Device Descriptor incompleto: actual=%u\n",
            actual);
        goto fail;
    }

    /*
     * Para Full-Speed, o tamanho inicial do EP0 é 8.
     * O descritor informa o tamanho definitivo.
     */
    if (speed == 1)
    {
        uint8_t packet =
            ((uint8_t *)descriptor_dma.virt)[7];
        if (packet != 8 &&
            packet != 16 &&
            packet != 32 &&
            packet != 64)
        {
            kprintf(
                "[xHCI] MaxPacketSize0 invalido: %u\n",
                packet);
            goto fail;
        }
        if (packet != device->max_packet_size)
        {
            stage = "Evaluate Context EP0";
            result = xhci_device_update_ep0_packet_size(
                x,
                device,
                packet);
            if (result != 0)
                goto fail;
        }
    }
    stage = "GET_DESCRIPTOR 18 bytes";
    actual = 0;
    result = xhci_device_read_descriptor(
        x,
        device,
        &descriptor_dma,
        USB_DEVICE_DESC_SIZE,
        &actual);
    if (result != 0)
    {
        kprintf(
            "[xHCI] GET_DESCRIPTOR completo falhou: result=%d actual=%u\n",
            result,
            actual);
        goto fail;
    }
    if (actual < USB_DEVICE_DESC_SIZE)
    {
        kprintf(
            "[xHCI] Device Descriptor incompleto: actual=%u esperado=%u\n",
            actual,
            USB_DEVICE_DESC_SIZE);
        goto fail;
    }

    stage = "Parse Device Descriptor";
    usb_device_descriptor_t descriptor;
    memcpy(
        &descriptor,
        descriptor_dma.virt,
        sizeof(descriptor));
    result = xhci_device_parse_descriptor(device, &descriptor);
    if (result != 0)
    {
        kprintf(
            "[xHCI] Device Descriptor invalido: resultado=%d\n",
            result);
        goto fail;
    }
    
    xhci_dma_free(&descriptor_dma);
    return 0;
fail:
    xhci_dma_free(&descriptor_dma);
    kprintf(
        "[xHCI] Enumeracao falhou: etapa=%s porta=%u slot=%u\n",
        stage,
        port_id,
        slot_id);
    xhci_device_disconnect(x, slot_id);
    return -5;
}
void xhci_device_disconnect(
    xhci_controller_t *x,
    uint8_t slot_id)
{
    if (!x || !slot_id ||
        slot_id > x->caps.max_slots)
        return;
    xhci_device_t *device = &x->devices[slot_id];
    if (!device->connected &&
        !device->context.initialized &&
        !device->enabled)
        return;
    uint8_t was_enabled = device->enabled;
    uint8_t context_initialized =
        device->context.initialized;
    device->connected = 0;
    device->addressed = 0;
    if (was_enabled)
    {
        int result = xhci_disable_slot(x, slot_id);
        if (result != 0)
        {
            kprintf(
                "[xHCI] Falha ao desativar slot %u: resultado=%d; contexto preservado\n",
                slot_id,
                result);
            device->connected = 0;
            return;
        }
    }
    device->enabled = 0;
    if (context_initialized)
        xhci_context_destroy(x, device);
    if (device->ep0_ring.initialized)
        xhci_dma_free(&device->ep0_ring.dma);
    memset(device, 0, sizeof(*device));
}