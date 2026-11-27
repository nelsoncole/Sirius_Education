#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>
#include <kernel/kvmm.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define TRB_TYPE_NORMAL       1U
#define TRB_TYPE_SETUP        2U
#define TRB_TYPE_DATA         3U
#define TRB_TYPE_STATUS       4U

#define TRB_CHAIN             (1U << 4)
#define TRB_IOC               (1U << 5)
#define TRB_IDT               (1U << 6)
#define TRB_DIR_IN            (1U << 16)
#define TRB_ISP                (1U << 2)
#define TRB_SETUP_TRT_OUT     (2U << 16)
#define TRB_SETUP_TRT_IN      (3U << 16)

static inline void xhci_transfer_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static inline void xhci_transfer_pause(void)
{
    __builtin_ia32_pause();
}

static int xhci_transfer_ring_valid(
    xhci_controller_t *x,
    xhci_ring_t *ring
)
{
    if (!x || !ring || !ring->initialized)
        return -1;

    if (!ring->trbs || !ring->dma.virt)
        return -2;

    if (ring->enqueue >= XHCI_RING_TRBS - 1)
        return -3;

    if (ring->dma.size < PAGE_SIZE)
        return -4;

    if (ring->dma.phys & 0x3FULL)
        return -5;

    if (!x->caps.address_64bit && (ring->dma.phys >> 32))
        return -6;

    return 0;
}

int xhci_transfer_ring_init(
    xhci_controller_t *x,
    xhci_ring_t *ring
)
{
    if (!x || !ring)
        return -1;

    memset(ring, 0, sizeof(*ring));

    if (xhci_dma_alloc(&ring->dma, PAGE_SIZE) != 0)
        return -2;

    if (!ring->dma.virt || !ring->dma.phys ||
        ring->dma.size < PAGE_SIZE)
        return -3;

    if (ring->dma.phys & 0x3FULL)
        return -4;

    if (!x->caps.address_64bit && (ring->dma.phys >> 32))
        return -5;

    ring->trbs = (volatile xhci_trb_t *)ring->dma.virt;
    ring->enqueue = 0;
    ring->cycle = 1;

    memset(ring->dma.virt, 0, PAGE_SIZE);

    volatile xhci_trb_t *link =
        &ring->trbs[XHCI_RING_TRBS - 1];

    link->parameter = ring->dma.phys;
    link->status = 0;
    link->control =
        ((uint32_t)XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
        XHCI_TRB_TC |
        XHCI_TRB_CYCLE;

    xhci_transfer_barrier();

    ring->initialized = 1;
    return 0;
}

static int xhci_transfer_ring_push(
    xhci_ring_t *ring,
    const xhci_trb_t *source,
    uint64_t *trb_physical
)
{
    if (!ring || !source || !ring->initialized ||
        !ring->trbs || !ring->dma.virt)
        return -1;

    if (ring->enqueue >= XHCI_RING_TRBS - 1)
        return -2;

    uint16_t index = ring->enqueue;
    volatile xhci_trb_t *destination = &ring->trbs[index];

    uint32_t control = source->control;

    destination->parameter = source->parameter;
    destination->status = source->status;

    destination->control =
        (control & ~XHCI_TRB_CYCLE) |
        (ring->cycle ? 0U : XHCI_TRB_CYCLE);

    xhci_transfer_barrier();

    destination->control =
        (control & ~XHCI_TRB_CYCLE) |
        (ring->cycle ? XHCI_TRB_CYCLE : 0U);

    if (trb_physical)
    {
        *trb_physical =
            ring->dma.phys +
            ((uint64_t)index * sizeof(xhci_trb_t));
    }

    ring->enqueue++;

    if (ring->enqueue == XHCI_RING_TRBS - 1)
    {
        volatile xhci_trb_t *link =
            &ring->trbs[XHCI_RING_TRBS - 1];

        uint32_t link_control =
            ((uint32_t)XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
            XHCI_TRB_TC |
            (control & TRB_CHAIN) |
            (ring->cycle ? XHCI_TRB_CYCLE : 0U);

        /* Escrita única atómica no control do Link TRB (parameter e status já estão fixos) */
        xhci_transfer_barrier();
        link->control = link_control;

        ring->enqueue = 0;
        ring->cycle ^= 1U;
    }

    xhci_transfer_barrier();
    return 0;
}

static int xhci_transfer_begin(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint8_t endpoint_id
)
{
    if (!x || !slot_id || !endpoint_id)
        return -1;

    if (!x->running || !x->initialized)
        return -2;

    if (slot_id > x->caps.max_slots || endpoint_id > 31)
        return -3;

    uint32_t expected = 0;

    if (!__atomic_compare_exchange_n(
            &x->transfer_busy,
            &expected,
            1U,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED))
        return -4;

    if (__atomic_load_n(&x->transfer_faulted, __ATOMIC_ACQUIRE))
    {
        __atomic_store_n(&x->transfer_busy, 0U, __ATOMIC_RELEASE);
        return -5;
    }

    x->transfer_expected_trb = 0;
    x->transfer_completed_trb = 0;
    x->transfer_residual = 0;
    x->transfer_completion_code = 0;
    x->transfer_slot_id = slot_id;
    x->transfer_endpoint_id = endpoint_id;

    __atomic_store_n(
        &x->transfer_completed,
        0U,
        __ATOMIC_RELEASE
    );

    xhci_transfer_barrier();
    return 0;
}

static void xhci_transfer_abort(xhci_controller_t *x)
{
    if (!x)
        return;

    __atomic_store_n(&x->transfer_faulted, 1U, __ATOMIC_RELEASE);
    __atomic_store_n(&x->transfer_busy, 0U, __ATOMIC_RELEASE);
}

static void xhci_transfer_set_expected(
    xhci_controller_t *x,
    uint64_t expected_trb
)
{
    x->transfer_expected_trb = expected_trb;
    xhci_transfer_barrier();
}

static int xhci_transfer_wait(
    xhci_controller_t *x,
    uint64_t expected_trb,
    uint32_t *residual,
    uint8_t *completion_code
)
{
    uint32_t timeout = XHCI_TRANSFER_TIMEOUT;

    while (timeout--)
    {
        xhci_process_events(x);

        if (__atomic_load_n(
                &x->transfer_completed,
                __ATOMIC_ACQUIRE))
        {
            uint64_t completed_trb = x->transfer_completed_trb;
            uint32_t transferred_residual = x->transfer_residual;
            uint8_t code = x->transfer_completion_code;

            if (completed_trb != expected_trb)
            {
                kprintf(
                    "[xHCI] Transfer Event TRB invalido: esperado=%lx recebido=%lx completion=%u residual=%u\n",
                    (unsigned long)expected_trb,
                    (unsigned long)completed_trb,
                    (unsigned)code,
                    (unsigned)transferred_residual);

                __atomic_store_n(
                    &x->transfer_busy,
                    0U,
                    __ATOMIC_RELEASE);

                return -4;
            }

            if (residual)
                *residual = transferred_residual;

            if (completion_code)
                *completion_code = code;

            __atomic_store_n(
                &x->transfer_busy,
                0U,
                __ATOMIC_RELEASE
            );

            if (code == XHCI_CC_SUCCESS ||
                code == XHCI_CC_SHORT_PACKET)
                return 0;

            kprintf(
                "[xHCI] Transfer concluida com erro: completion=%u residual=%u endpoint=%u slot=%u trb=%lx\n",
                (unsigned)code,
                (unsigned)transferred_residual,
                (unsigned)x->transfer_endpoint_id,
                (unsigned)x->transfer_slot_id,
                (unsigned long)completed_trb
            );

            return -2;
        }

        xhci_transfer_pause();
    }

    kprintf(
        "[xHCI] Timeout Transfer Event: esperado=%lx slot=%u endpoint=%u\n",
        (unsigned long)expected_trb,
        (unsigned)x->transfer_slot_id,
        (unsigned)x->transfer_endpoint_id
    );

    xhci_transfer_abort(x);
    return -3;
}


static void xhci_transfer_ring_doorbell(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint8_t endpoint_id
)
{
    if (!x || !x->doorbells || !slot_id || endpoint_id > 31)
    {
return;
    }

    volatile uint32_t *doorbell =
        (volatile uint32_t *)(
            x->doorbells +
            ((uint32_t)slot_id * XHCI_DOORBELL_STRIDE)
        );

    kprintf(
        "[xHCI DOORBELL] slot=%u endpoint=%u register=%p value=%08x\n",
        (unsigned)slot_id,
        (unsigned)endpoint_id,
        (void *)doorbell,
        (unsigned)endpoint_id
    );

    xhci_transfer_barrier();
    *doorbell = endpoint_id;
    xhci_transfer_barrier();
}

static int xhci_transfer_validate_buffer(
    xhci_controller_t *x,
    const xhci_dma_t *buffer,
    uint32_t length
)
{
    if (!x || !buffer || !buffer->virt || !buffer->phys)
        return -1;

    if (!length || length > buffer->size)
        return -2;

    if (!x->caps.address_64bit && (buffer->phys >> 32))
        return -3;

    uintptr_t virtual_address = (uintptr_t)buffer->virt;
    uintptr_t translated = vmm_get_physical(virtual_address);
if (translated != buffer->phys)
        return -4;

    if (length > 1)
    {
        uintptr_t last_virtual = virtual_address + length - 1u;
        uintptr_t last_physical = vmm_get_physical(last_virtual);

        if (last_physical != buffer->phys + length - 1u)
        {
            kprintf(
                "[xHCI DMA BUFFER] WARNING: buffer nao contiguo "
                "last_virt=%p last_phys=%lx expected=%lx\n",
                (void *)last_virtual,
                (unsigned long)last_physical,
                (unsigned long)(buffer->phys + length - 1u)
            );
            return -5;
        }
    }

    return 0;
}

static void xhci_debug_dma_scratch(
    xhci_dma_t *buffer,
    uint32_t length,
    const char *stage
)
{
    if (!buffer || !buffer->phys || !length)
        return;

    kprintf(
        "[xHCI SCRATCH] %s PHYS=%lx LENGTH=%u\n",
        stage,
        (unsigned long)buffer->phys,
        length
    );

    uint8_t *original = (uint8_t *)buffer->virt;

    kprintf("[xHCI SCRATCH] ORIGINAL:");

    for (uint32_t i = 0; i < length && i < 18; i++)
        kprintf(" %02x", original[i]);

    kprintf("\n");

    uintptr_t page_phys =
        (uintptr_t)buffer->phys & ~(uintptr_t)(PAGE_SIZE - 1);

    uint32_t offset =
        (uint32_t)(buffer->phys - page_phys);

    uint8_t *scratch =
        (uint8_t *)vmm_scratch_map((unsigned long)page_phys);

    if (!scratch)
    {
        kprintf("[xHCI SCRATCH] FALHA NO MAPEAMENTO\n");
        return;
    }

    uint8_t *mapped = scratch + offset;

    kprintf(
        "[xHCI SCRATCH] VIRTUAL=%p OFFSET=%u\n",
        mapped,
        offset
    );

    kprintf("[xHCI SCRATCH] PHYSICAL DATA:");

    for (uint32_t i = 0; i < length && i < 18; i++)
        kprintf(" %02x", mapped[i]);

    kprintf("\n");

    kprintf(
        "[xHCI SCRATCH] COMPARE=%s\n",
        memcmp(original, mapped, length < 18 ? length : 18) == 0
            ? "MATCH"
            : "DIFFERENT"
    );
}

int xhci_control_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    const xhci_setup_packet_t *setup,
    xhci_dma_t *buffer,
    uint32_t length,
    uint8_t direction_in,
    uint32_t *actual_length
)
{
    if (!x || !device || !setup)
        return -1;

    if (sizeof(xhci_setup_packet_t) != 8)
        return -2;

    if (!device->context.initialized ||
        !device->context.slot_id ||
        !device->addressed)
        return -3;

    if (!device->ep0_ring.initialized)
        return -4;

    if (length && xhci_transfer_validate_buffer(x, buffer, length) != 0)
        return -5;

    if (length != setup->length)
        return -6;

    if (length > 0x1FFFFU)
        return -7;

    if (length && direction_in && !(setup->request_type & 0x80U))
        return -8;

    if (length && !direction_in && (setup->request_type & 0x80U))
        return -9;

    if (actual_length)
        *actual_length = 0;

    xhci_ring_t *ring = &device->ep0_ring;

    if (xhci_transfer_ring_valid(x, ring) != 0)
        return -10;

    int result = xhci_transfer_begin(
        x,
        device->context.slot_id,
        1
    );

    if (result != 0)
        return -11;

    xhci_trb_t trb;
    uint64_t setup_trb_phys = 0;
    uint64_t data_trb_phys = 0;
    uint64_t status_trb_phys = 0;

    memset(&trb, 0, sizeof(trb));
    memcpy(&trb.parameter, setup, sizeof(*setup));
    trb.status = 8;

    trb.control =
        ((uint32_t)TRB_TYPE_SETUP << XHCI_TRB_TYPE_SHIFT) |
        TRB_IDT |
        TRB_CHAIN;

    if (length)
    {
        trb.control |= direction_in ?
            TRB_SETUP_TRT_IN :
            TRB_SETUP_TRT_OUT;
    }

    if (xhci_transfer_ring_push(
            ring,
            &trb,
            &setup_trb_phys) != 0)
    {
        xhci_transfer_abort(x);
        return -12;
    }

    if (length)
    {
        memset(&trb, 0, sizeof(trb));

        trb.parameter = buffer->phys;
        trb.status = length;

        trb.control =
            ((uint32_t)TRB_TYPE_DATA << XHCI_TRB_TYPE_SHIFT) |
            TRB_CHAIN;
        if (direction_in)
            trb.control |= TRB_DIR_IN | TRB_ISP;
        if (xhci_transfer_ring_push(
                ring,
                &trb,
                &data_trb_phys) != 0)
        {
            xhci_transfer_abort(x);
            return -13;
        }
    }
    memset(&trb, 0, sizeof(trb));
    trb.control =
        ((uint32_t)TRB_TYPE_STATUS << XHCI_TRB_TYPE_SHIFT) |
        TRB_IOC;
    if (!length || !direction_in)
        trb.control |= TRB_DIR_IN;
    if (xhci_transfer_ring_push(
            ring,
            &trb,
            &status_trb_phys) != 0)
    {
        xhci_transfer_abort(x);
        return -14;
    }
    xhci_transfer_set_expected(x, status_trb_phys);

    if (length && direction_in)
        xhci_debug_dma_scratch(buffer, length, "ANTES");

    xhci_transfer_barrier();

    xhci_transfer_ring_doorbell(
        x,
        device->context.slot_id,
        1
    );
    uint32_t residual = 0;
    uint8_t completion_code = 0;
    result = xhci_transfer_wait(
        x,
        status_trb_phys,
        &residual,
        &completion_code);

    xhci_transfer_barrier();

    if (length && direction_in)
        xhci_debug_dma_scratch(buffer, length, "DEPOIS");
    kprintf(
        "[xHCI CONTROL] result=%d CC=%u residual=%u actual=%u\n",
        result,
        (unsigned)completion_code,
        (unsigned)residual,
        (unsigned)(residual <= length ? length - residual : 0)
    );
if (result != 0)
    {
        kprintf(
            "[xHCI] Control Transfer falhou: wait=%d completion=%u residual=%u setup=%02x/%02x length=%u\n",
            result,
            (unsigned)completion_code,
            (unsigned)residual,
            setup->request_type,
            setup->request,
            (unsigned)length);
        return -15;
    }
    if (residual > length)
        return -16;
    if (actual_length)
        *actual_length = length - residual;
    return 0;
}
int xhci_bulk_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    xhci_ring_t *ring,
    uint8_t endpoint_id,
    xhci_dma_t *buffer,
    uint32_t length,
    uint32_t *actual_length)
{
    if (!x || !device || !ring || !buffer)
        return -1;
    if (!device->context.initialized ||
        !device->addressed)
        return -2;
    if (!endpoint_id || endpoint_id > 31)
        return -3;
    if (xhci_transfer_ring_valid(x, ring) != 0)
        return -4;
    if (xhci_transfer_validate_buffer(x, buffer, length) != 0)
        return -5;
    if (actual_length)
        *actual_length = 0;
    int result = xhci_transfer_begin(
        x,
        device->context.slot_id,
        endpoint_id);
    if (result != 0)
        return -6;
    xhci_trb_t trb;
    uint64_t expected_trb = 0;
    memset(&trb, 0, sizeof(trb));
    trb.parameter = buffer->phys;
    trb.status = length;
    trb.control =
        ((uint32_t)TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
        TRB_IOC;
    if (xhci_transfer_ring_push(
            ring,
            &trb,
            &expected_trb) != 0)
    {
        xhci_transfer_abort(x);
        return -7;
    }
    xhci_transfer_set_expected(x, expected_trb);
    xhci_transfer_ring_doorbell(
        x,
        device->context.slot_id,
        endpoint_id);
    uint32_t residual = 0;
    uint8_t completion_code = 0;
    result = xhci_transfer_wait(
        x,
        expected_trb,
        &residual,
        &completion_code);
    if (result != 0)
        return -8;
    if (residual > length)
        return -9;
    if (actual_length)
        *actual_length = length - residual;
    return 0;
}
int xhci_interrupt_transfer(
    xhci_controller_t *x,
    xhci_device_t *device,
    xhci_ring_t *ring,
    uint8_t endpoint_id,
    xhci_dma_t *buffer,
    uint32_t length,
    uint32_t *actual_length)
{
    return xhci_bulk_transfer(
        x,
        device,
        ring,
        endpoint_id,
        buffer,
        length,
        actual_length);
}

