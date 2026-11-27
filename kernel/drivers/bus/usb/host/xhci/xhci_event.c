#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define XHCI_EVENT_BUDGET XHCI_EVENT_TRBS

static inline void xhci_event_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static inline uint32_t xhci_event_read32(volatile uint8_t *base, uint32_t offset)
{
    return *(volatile uint32_t *)(base + offset);
}

static inline void xhci_event_write32(volatile uint8_t *base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

static inline void xhci_event_write64(volatile uint8_t *base, uint32_t offset, uint64_t value)
{
    *(volatile uint64_t *)(base + offset) = value;
}

int xhci_event_ring_init(xhci_controller_t *x)
{
    if (!x || !x->runtime || !x->caps.max_interrupters)
        return -1;

    xhci_event_ring_t *ring = &x->event_ring;
    memset(ring, 0, sizeof(*ring));

    if (xhci_dma_alloc(&ring->ring_dma, PAGE_SIZE) != 0)
        return -2;

    if (xhci_dma_alloc(&ring->erst_dma, PAGE_SIZE) != 0)
        return -3;

    if (!ring->ring_dma.virt || !ring->erst_dma.virt ||
        !ring->ring_dma.phys || !ring->erst_dma.phys)
        return -4;

    if ((ring->ring_dma.phys & 0x3Fu) ||
        (ring->erst_dma.phys & 0x3Fu))
        return -5;

    if (!x->caps.address_64bit &&
        ((ring->ring_dma.phys >> 32) ||
         (ring->erst_dma.phys >> 32)))
        return -6;

    ring->trbs = (volatile xhci_trb_t *)ring->ring_dma.virt;
    ring->dequeue = 0;
    ring->cycle = 1;
    ring->size = XHCI_EVENT_TRBS;

    memset((void *)ring->trbs, 0, PAGE_SIZE);
    memset(ring->erst_dma.virt, 0, PAGE_SIZE);

    xhci_erst_entry_t *erst =
        (xhci_erst_entry_t *)ring->erst_dma.virt;

    erst[0].base = ring->ring_dma.phys;
    erst[0].size = ring->size;
    erst[0].reserved = 0;

    volatile uint8_t *intr = x->runtime + XHCI_INTR0;

    xhci_event_write32(intr, XHCI_IMAN, XHCI_IMAN_IP);
    xhci_event_write32(intr, XHCI_IMOD, 0);
    xhci_event_write32(intr, XHCI_ERSTSZ, 1);
    xhci_event_write64(intr, XHCI_ERSTBA, ring->erst_dma.phys);
    xhci_event_write64(intr, XHCI_ERDP, ring->ring_dma.phys);

    xhci_event_barrier();
    ring->initialized = 1;

    kprintf(
        "[xHCI EVENT] ring=%lx erst=%lx entries=%u\n",
        (unsigned long)ring->ring_dma.phys,
        (unsigned long)ring->erst_dma.phys,
        ring->size
    );

    return 0;
}

static void xhci_handle_command_completion(
    xhci_controller_t *x,
    const volatile xhci_trb_t *event
)
{
    uint64_t pointer = event->parameter;
    uint32_t status = event->status;
    uint32_t control = event->control;

    if (!__atomic_load_n(&x->command.busy, __ATOMIC_ACQUIRE))
        return;

    if (pointer != x->command.expected_trb)
    {
        kprintf(
            "[xHCI EVENT] comando inesperado ptr=%lx esperado=%lx\n",
            (unsigned long)pointer,
            (unsigned long)x->command.expected_trb
        );
        return;
    }

    x->command.completed_trb = pointer;
    x->command.completion_code = (uint8_t)(status >> 24);
    x->command.slot_id =
        (uint8_t)(control >> XHCI_TRB_SLOT_ID_SHIFT);
    x->command.residual = status & 0x00FFFFFFu;

    __atomic_store_n(&x->command.completed, 1U, __ATOMIC_RELEASE);
}

static void xhci_handle_port_status_change(
    xhci_controller_t *x,
    const volatile xhci_trb_t *event
)
{
    uint8_t port_id = (uint8_t)(event->parameter >> 24);

    if (!port_id || port_id > x->caps.max_ports)
        return;

    uint32_t index = ((uint32_t)port_id - 1u) / 32u;
    uint32_t bit = 1u << (((uint32_t)port_id - 1u) % 32u);

    __atomic_fetch_or(
        &x->port_change_pending[index],
        bit,
        __ATOMIC_RELEASE
    );
}

static void xhci_handle_transfer_event(
    xhci_controller_t *x,
    const volatile xhci_trb_t *event
)
{
    uint64_t pointer = event->parameter;
    uint32_t status = event->status;
    uint8_t completion = (uint8_t)(status >> 24);
    uint32_t residual = status & 0x00FFFFFFu;

    uint8_t slot_id =
        (uint8_t)(event->control >> XHCI_TRB_SLOT_ID_SHIFT);

    uint8_t endpoint_id =
        (uint8_t)((event->control >> XHCI_TRB_ENDPOINT_ID_SHIFT) & 0x1Fu);

    uint8_t trb_type = (uint8_t)((event->control >> 10) & 0x3F);

    kprintf(
        "[xHCI EVENT RAW] PTR=%lx STATUS=%08x CONTROL=%08x "
        "TYPE=%u CC=%u RESIDUAL=%u SLOT=%u EP=%u\n",
        (unsigned long)pointer,
        event->status,
        event->control,
        trb_type,
        completion,
        residual,
        slot_id,
        endpoint_id
    );

    if (!__atomic_load_n(&x->transfer_busy, __ATOMIC_ACQUIRE))
    {
        kprintf(
            "[xHCI TRANSFER EVENT] ignorado: sem transferencia activa "
            "ptr=%lx CC=%u slot=%u ep=%u\n",
            (unsigned long)pointer,
            completion,
            slot_id,
            endpoint_id
        );
        return;
    }

    if (slot_id != x->transfer_slot_id ||
        endpoint_id != x->transfer_endpoint_id)
    {
        kprintf(
            "[xHCI TRANSFER EVENT] ignorado: slot/endpoint diferente "
            "ptr=%lx slot=%u/%u ep=%u/%u\n",
            (unsigned long)pointer,
            slot_id,
            x->transfer_slot_id,
            endpoint_id,
            x->transfer_endpoint_id
        );
        return;
    }

    uint64_t expected =
        __atomic_load_n(&x->transfer_expected_trb, __ATOMIC_ACQUIRE);

    kprintf(
        "[xHCI TRANSFER EVENT] ptr=%lx esperado=%lx CC=%u residual=%u "
        "slot=%u ep=%u\n",
        (unsigned long)pointer,
        (unsigned long)expected,
        completion,
        residual,
        slot_id,
        endpoint_id
    );

    /*
     * Para o Control TD actual, o software espera a conclusao do Status TRB.
     * Um evento que aponte para o Data TRB (por exemplo, Short Packet com ISP)
     * e intermédio: deve ser registado, mas nao pode finalizar o wait().
     */
    if (!expected || pointer != expected)
    {
        kprintf(
            "[xHCI TRANSFER EVENT] evento intermedio/nao esperado; "
            "transferencia continua activa\n"
        );
        return;
    }

    x->transfer_completed_trb = pointer;
    x->transfer_residual = residual;
    x->transfer_completion_code = completion;

    __atomic_store_n(
        &x->transfer_completed,
        1U,
        __ATOMIC_RELEASE
    );
    
}

void xhci_process_events(xhci_controller_t *x)
{
    if (!x || !x->event_ring.initialized ||
        !x->runtime || !x->event_ring.trbs)
        return;

    unsigned long flags = spinlock_lock_irqsave(&x->lock);

    xhci_event_ring_t *ring = &x->event_ring;
    volatile uint8_t *intr = x->runtime + XHCI_INTR0;
    uint32_t processed = 0;

    while (processed < XHCI_EVENT_BUDGET)
    {
        volatile xhci_trb_t *event =
            &ring->trbs[ring->dequeue];

        xhci_event_barrier();

        uint32_t control = event->control;

        if ((control & XHCI_TRB_CYCLE) != ring->cycle)
            break;

        uint32_t type = XHCI_TRB_TYPE(control);

        switch (type)
        {
            case XHCI_TRB_TYPE_COMMAND_COMPLETION:
                xhci_handle_command_completion(x, event);
                break;

            case XHCI_TRB_TYPE_PORT_STATUS_CHANGE:
                xhci_handle_port_status_change(x, event);
                break;

            case XHCI_TRB_TYPE_TRANSFER_EVENT:
                xhci_handle_transfer_event(x, event);
                break;

            default:
                kprintf(
                    "[xHCI EVENT] tipo desconhecido=%u index=%u "
                    "parameter=%lx status=%08x control=%08x\n",
                    type,
                    ring->dequeue,
                    (unsigned long)event->parameter,
                    event->status,
                    event->control
                );
                break;
        }

        ring->dequeue++;

        if (ring->dequeue >= ring->size)
        {
            ring->dequeue = 0;
            ring->cycle ^= 1U;
        }

        processed++;
    }

    uint64_t dequeue_phys =
        ring->ring_dma.phys +
        ((uint64_t)ring->dequeue * sizeof(xhci_trb_t));

    if (processed)
    {
        xhci_event_barrier();
        xhci_event_write64(
            intr,
            XHCI_ERDP,
            dequeue_phys | XHCI_ERDP_EHB
        );
    }

    uint32_t iman = xhci_event_read32(intr, XHCI_IMAN);

    if (iman & XHCI_IMAN_IP)
    {
        xhci_event_write32(
            intr,
            XHCI_IMAN,
            (iman & XHCI_IMAN_IE) | XHCI_IMAN_IP
        );
    }

    xhci_event_barrier();
    spinlock_unlock_irqrestore(&x->lock, flags);
}

void xhci_interrupt_handler(void)
{
    xhci_controller_t *x = &g_xhci_controller;

    if (!x->initialized || !x->op ||
        !x->runtime || !x->event_ring.initialized)
        return;

    uint32_t status = xhci_event_read32(x->op, XHCI_USBSTS);

    uint32_t pending =
        status & (XHCI_STS_EINT | XHCI_STS_PCD | XHCI_STS_HSE);

    if (!pending)
        return;

    xhci_event_write32(x->op, XHCI_USBSTS, pending);
    xhci_event_barrier();

    if (pending & (XHCI_STS_EINT | XHCI_STS_PCD))
        xhci_process_events(x);
}
