
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: xhci_ring.c
 *    Description: Gestao do Command Ring do controlador xHCI.
 *                 Inicializa o anel, publica comandos, gere o Cycle Bit,
 *                 notifica o controlador e aguarda eventos de conclusao.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *
 *        License: MIT
 * ============================================================================
 */
#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define XHCI_RING_COMMAND_TIMEOUT 10000000u

static inline void xhci_ring_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static inline void xhci_ring_pause(void)
{
    __builtin_ia32_pause();
}

static void xhci_ring_update_link(xhci_ring_t *ring)
{
    volatile xhci_trb_t *link = &ring->trbs[XHCI_RING_TRBS - 1];
    link->parameter = ring->dma.phys;
    link->status = 0;
    uint32_t control = ((uint32_t)XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) |
                      XHCI_TRB_TC;
    link->control = control;
    xhci_ring_barrier();
    link->control = control | (ring->cycle ? XHCI_TRB_CYCLE : 0);
}

static int xhci_ring_validate(const xhci_ring_t *ring)
{
    if (!ring || !ring->trbs || !ring->dma.virt || !ring->initialized)
        return -1;
    if (ring->dma.size < PAGE_SIZE || ring->enqueue >= XHCI_RING_TRBS - 1)
        return -2;
    if (ring->dma.phys & 0x3Fu)
        return -3;
    return 0;
}

int xhci_ring_init(xhci_controller_t *x)
{
    if (!x)
        return -1;

    xhci_ring_t *ring = &x->command_ring;
    memset(ring, 0, sizeof(*ring));
    if (xhci_dma_alloc(&ring->dma, PAGE_SIZE) != 0)
        return -2;
    if (!ring->dma.virt || !ring->dma.phys || (ring->dma.phys & 0x3Fu))
        return -3;
    if (!x->caps.address_64bit && (ring->dma.phys >> 32))
        return -4;

    ring->trbs = (volatile xhci_trb_t *)ring->dma.virt;
    memset((void *)ring->trbs, 0, PAGE_SIZE);
    ring->enqueue = 0;
    ring->cycle = 1;
    ring->initialized = 1;
    xhci_ring_update_link(ring);

    x->command.busy = 0;
    x->command.completed = 0;
    x->command.faulted = 0;
    x->command.expected_trb = 0;
    x->command.completed_trb = 0;
    x->command.residual = 0;
    x->command.completion_code = 0;
    x->command.slot_id = 0;
    xhci_ring_barrier();
    return 0;
}

int xhci_command_submit(xhci_controller_t *x, const xhci_trb_t *command,
                        uint32_t timeout, uint8_t *slot_id,
                        uint8_t *completion_code)
{
    if (!x || !command)
        return -1;
    if (!x->initialized || !x->running || !x->doorbells)
        return -2;
    if (xhci_ring_validate(&x->command_ring) != 0)
        return -3;
    if (__atomic_load_n(&x->command.faulted, __ATOMIC_ACQUIRE))
        return -4;

    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&x->command.busy, &expected, 1, 0,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return -5;

    xhci_ring_t *ring = &x->command_ring;
    uint16_t index = ring->enqueue;
    volatile xhci_trb_t *trb = &ring->trbs[index];
    uint64_t trb_phys = ring->dma.phys + (uint64_t)index * sizeof(xhci_trb_t);

    __atomic_store_n(&x->command.completed, 0, __ATOMIC_RELAXED);
    x->command.expected_trb = trb_phys;
    x->command.completed_trb = 0;
    x->command.completion_code = 0;
    x->command.slot_id = 0;
    x->command.residual = 0;

    trb->parameter = command->parameter;
    trb->status = command->status;
    trb->control = command->control & ~XHCI_TRB_CYCLE;
    xhci_ring_barrier();
    trb->control = (command->control & ~XHCI_TRB_CYCLE) |
                   (ring->cycle ? XHCI_TRB_CYCLE : 0);

    ring->enqueue++;
    if (ring->enqueue == XHCI_RING_TRBS - 1)
    {
        xhci_ring_update_link(ring);
        ring->enqueue = 0;
        ring->cycle ^= 1u;
    }

    xhci_ring_barrier();
    *(volatile uint32_t *)(x->doorbells + XHCI_DOORBELL0) = 0;
    xhci_ring_barrier();

    if (!timeout)
        timeout = XHCI_RING_COMMAND_TIMEOUT;

    while (timeout--)
    {
        /*
         * Polling de segurança: algumas plataformas/firmwares não entregam
         * a interrupção legacy/MSI de forma fiável. O Event Ring continua a
         * ser a fonte oficial da conclusão, mas é consumido também aqui.
         */
        if ((timeout & 0xFFFu) == 0)
            xhci_process_events(x);

        if (__atomic_load_n(&x->command.completed, __ATOMIC_ACQUIRE))
        {
            if (x->command.completed_trb != trb_phys)
                break;

            uint8_t code = x->command.completion_code;
            if (slot_id)
                *slot_id = x->command.slot_id;
            if (completion_code)
                *completion_code = code;
            __atomic_store_n(&x->command.busy, 0, __ATOMIC_RELEASE);
            return code == XHCI_CC_SUCCESS ? 0 : -6;
        }
        xhci_ring_pause();
    }

    __atomic_store_n(&x->command.faulted, 1, __ATOMIC_RELEASE);

    uint32_t usbsts = x->op ? *(volatile uint32_t *)(x->op + XHCI_USBSTS) : 0;
    uint32_t iman = x->runtime ?
        *(volatile uint32_t *)(x->runtime + XHCI_INTR0 + XHCI_IMAN) : 0;
    uint64_t erdp = x->runtime ?
        *(volatile uint64_t *)(x->runtime + XHCI_INTR0 + XHCI_ERDP) : 0;

    kprintf("[xHCI] Command Ring timeout: USBSTS=%x IMAN=%x ERDP=%lx\n",
            usbsts, iman, (unsigned long)erdp);
    return -7;
}
