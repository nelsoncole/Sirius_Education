#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/drivers/bus/pci/pci.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>
#include <kernel/arch/x86_64/kapi/irq.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define XHCI_MAP_SIZE       0x10000u
#define XHCI_INIT_TIMEOUT   10000000u

xhci_controller_t g_xhci_controller;

static inline uint32_t xhci_read32(volatile uint8_t *base, uint32_t offset)
{
    return *(volatile uint32_t *)(base + offset);
}

static inline void xhci_write32(volatile uint8_t *base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

static inline void xhci_write64(volatile uint8_t *base, uint32_t offset, uint64_t value)
{
    *(volatile uint64_t *)(base + offset) = value;
}

static inline void xhci_memory_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static inline void xhci_cpu_pause(void)
{
    __builtin_ia32_pause();
}

static int xhci_wait_register(volatile uint8_t *base, uint32_t offset,
                              uint32_t mask, uint32_t expected, uint32_t timeout)
{
    while (timeout--)
    {
        if ((xhci_read32(base, offset) & mask) == expected)
            return 0;
        xhci_cpu_pause();
    }
    return -1;
}

static int xhci_get_mmio_base(pci_device_t *dev, uint64_t *physical)
{
    if (!dev || !physical)
        return -1;

    uint32_t bar0 = dev->bar[0];
    if (!bar0 || (bar0 & 1u))
        return -2;

    uint32_t type = (bar0 >> 1) & 3u;
    if (type == 2u)
        *physical = ((uint64_t)dev->bar[1] << 32) | (bar0 & ~0xFULL);
    else if (type == 0u)
        *physical = (uint64_t)(bar0 & ~0xFULL);
    else
        return -3;

    return *physical ? 0 : -4;
}

static int xhci_read_capabilities(xhci_controller_t *x)
{
    if (!x || !x->mmio)
        return -1;

    x->caps.cap_length = x->mmio[XHCI_CAP_CAPLENGTH];
    if (x->caps.cap_length < 0x20)
        return -2;

    x->caps.version = *(volatile uint16_t *)(x->mmio + XHCI_CAP_HCIVERSION);
    x->caps.hcsparams1 = xhci_read32(x->mmio, XHCI_CAP_HCSPARAMS1);
    x->caps.hcsparams2 = xhci_read32(x->mmio, XHCI_CAP_HCSPARAMS2);
    x->caps.hcsparams3 = xhci_read32(x->mmio, XHCI_CAP_HCSPARAMS3);
    x->caps.hccparams1 = xhci_read32(x->mmio, XHCI_CAP_HCCPARAMS1);
    x->caps.hccparams2 = xhci_read32(x->mmio, XHCI_CAP_HCCPARAMS2);
    x->caps.dboff = xhci_read32(x->mmio, XHCI_CAP_DBOFF) & ~3u;
    x->caps.rtsoff = xhci_read32(x->mmio, XHCI_CAP_RTSOFF) & ~0x1Fu;

    x->caps.max_slots = (uint8_t)XHCI_HCSPARAMS1_MAX_SLOTS(x->caps.hcsparams1);
    x->caps.max_interrupters = (uint16_t)XHCI_HCSPARAMS1_MAX_INTRS(x->caps.hcsparams1);
    x->caps.max_ports = (uint8_t)XHCI_HCSPARAMS1_MAX_PORTS(x->caps.hcsparams1);
    x->caps.scratchpad_count = (uint16_t)XHCI_HCSPARAMS2_SCRATCHPAD_COUNT(x->caps.hcsparams2);
    x->caps.address_64bit = (x->caps.hccparams1 & 1u) ? 1 : 0;
    x->caps.context_size = (x->caps.hccparams1 & (1u << 2)) ? 64 : 32;

    if (!x->caps.max_slots || !x->caps.max_ports || !x->caps.max_interrupters)
        return -3;
    if (!x->caps.dboff || !x->caps.rtsoff)
        return -4;
    return 0;
}

static int xhci_validate_mmio_layout(xhci_controller_t *x)
{
    if (!x || !x->mmio)
        return -1;

    uint32_t op_end = (uint32_t)x->caps.cap_length + XHCI_PORTSC_BASE +
                      ((uint32_t)x->caps.max_ports - 1u) * XHCI_PORT_STRIDE + 4u;
    uint32_t runtime_end = x->caps.rtsoff + XHCI_INTR0 + XHCI_ERDP + 8u;
    uint32_t doorbell_end = x->caps.dboff +
                            ((uint32_t)x->caps.max_slots + 1u) * XHCI_DOORBELL_STRIDE;

    if (op_end > XHCI_MAP_SIZE || runtime_end > XHCI_MAP_SIZE ||
        doorbell_end > XHCI_MAP_SIZE)
        return -2;
    return 0;
}


static int xhci_legacy_handoff(xhci_controller_t *x)
{
    if (!x || !x->mmio)
        return -1;

    uint32_t offset =
        ((x->caps.hccparams1 >> 16) & 0xFFFFu) * 4u;

    if (!offset)
        return 0;

    for (uint32_t n = 0; n < 64; n++)
    {
        if (offset > XHCI_MAP_SIZE - 8u)
            return -2;

        uint32_t header = xhci_read32(x->mmio, offset);

        uint8_t id = (uint8_t)header;
        uint8_t next = (uint8_t)(header >> 8);

        if (id == XHCI_LEGACY_CAP_ID)
        {
            xhci_write32(
                x->mmio,
                offset,
                header | XHCI_OS_OWNED
            );

            xhci_memory_barrier();

            uint32_t ownership =
                xhci_read32(x->mmio, offset);

            if (!(ownership & XHCI_OS_OWNED))
                return -3;

            if (ownership & XHCI_BIOS_OWNED)
            {
                if (xhci_wait_register(
                    x->mmio,
                    offset,
                    XHCI_BIOS_OWNED,
                    0,
                    XHCI_INIT_TIMEOUT
                ) != 0)
                    return -4;
            }

            xhci_write32(x->mmio, offset + 4u, 0);
            xhci_memory_barrier();

            return 0;
        }

        if (!next)
            break;

        offset += (uint32_t)next * 4u;
    }

    return 0;
}

static int xhci_reset_controller(xhci_controller_t *x)
{
    if (!x || !x->op)
        return -1;

    uint32_t command = xhci_read32(x->op, XHCI_USBCMD) & ~XHCI_CMD_RUN;
    xhci_write32(x->op, XHCI_USBCMD, command);
    if (xhci_wait_register(x->op, XHCI_USBSTS, XHCI_STS_HALTED,
                           XHCI_STS_HALTED, XHCI_INIT_TIMEOUT) != 0)
        return -2;

    xhci_write32(x->op, XHCI_USBCMD,
                 xhci_read32(x->op, XHCI_USBCMD) | XHCI_CMD_RESET);
    if (xhci_wait_register(x->op, XHCI_USBCMD, XHCI_CMD_RESET, 0,
                           XHCI_INIT_TIMEOUT) != 0)
        return -3;
    if (xhci_wait_register(x->op, XHCI_USBSTS, XHCI_STS_CNR, 0,
                           XHCI_INIT_TIMEOUT) != 0)
        return -4;

    uint32_t status = xhci_read32(x->op, XHCI_USBSTS);
    xhci_write32(x->op, XHCI_USBSTS, status &
                 (XHCI_STS_EINT | XHCI_STS_PCD | XHCI_STS_HSE));
    xhci_memory_barrier();
    return 0;
}

static int xhci_setup_scratchpads(xhci_controller_t *x)
{
    uint16_t count = x->caps.scratchpad_count;
    if (!count)
        return 0;

    if (count > 1023u)
        return -1;

    x->scratchpads.buffers = (xhci_dma_t *)pool_alloc((size_t)count * sizeof(xhci_dma_t));
    if (!x->scratchpads.buffers)
        return -2;
    memset(x->scratchpads.buffers, 0, (size_t)count * sizeof(xhci_dma_t));

    if (xhci_dma_alloc(&x->scratchpads.array_dma, (size_t)count * sizeof(uint64_t)) != 0)
        return -3;

    volatile uint64_t *array = (volatile uint64_t *)x->scratchpads.array_dma.virt;
    for (uint16_t i = 0; i < count; i++)
    {
        if (xhci_dma_alloc(&x->scratchpads.buffers[i], PAGE_SIZE) != 0)
            return -4;
        if (!x->caps.address_64bit &&
            ((x->scratchpads.array_dma.phys >> 32) ||
             (x->scratchpads.buffers[i].phys >> 32)))
            return -5;
        array[i] = x->scratchpads.buffers[i].phys;
    }

    volatile uint64_t *dcbaa = (volatile uint64_t *)x->dcbaa.virt;
    dcbaa[0] = x->scratchpads.array_dma.phys;
    x->scratchpads.count = count;
    x->scratchpads.initialized = 1;
    xhci_memory_barrier();
    return 0;
}

static int xhci_setup_dma(xhci_controller_t *x)
{
    if (xhci_dma_alloc(&x->dcbaa, PAGE_SIZE) != 0)
        return -1;
    if (!x->caps.address_64bit && (x->dcbaa.phys >> 32))
        return -2;
    if (xhci_context_init(x) != 0)
        return -3;
    if (xhci_setup_scratchpads(x) != 0)
        return -4;
    if (xhci_ring_init(x) != 0)
        return -5;
    if (xhci_event_ring_init(x) != 0)
        return -6;

    if (!x->caps.address_64bit &&
        ((x->command_ring.dma.phys >> 32) ||
         (x->event_ring.ring_dma.phys >> 32) ||
         (x->event_ring.erst_dma.phys >> 32)))
        return -7;
    return 0;
}

static int xhci_start_controller(xhci_controller_t *x)
{
    if (!x || !x->op || !x->runtime || !x->doorbells ||
        !x->dcbaa.virt || !x->command_ring.initialized ||
        !x->event_ring.initialized)
        return -1;

    xhci_write64(x->op, XHCI_DCBAAP, x->dcbaa.phys);
    xhci_write64(x->op, XHCI_CRCR, x->command_ring.dma.phys | 1ULL);
    xhci_write32(x->op, XHCI_CONFIG, x->caps.max_slots);
    xhci_memory_barrier();

    x->initialized = 1;
    kapi_register_irq_handler(x->pci, xhci_interrupt_handler);

    volatile uint8_t *intr = x->runtime + XHCI_INTR0;
    uint32_t iman = xhci_read32(intr, XHCI_IMAN);
    xhci_write32(intr, XHCI_IMAN, (iman & XHCI_IMAN_IE) | XHCI_IMAN_IP | XHCI_IMAN_IE);
    xhci_write32(intr, XHCI_IMOD, 0);

    uint32_t command = xhci_read32(x->op, XHCI_USBCMD);
    xhci_write32(x->op, XHCI_USBCMD, command | XHCI_CMD_INTE | XHCI_CMD_RUN);
    xhci_memory_barrier();

    if (xhci_wait_register(x->op, XHCI_USBSTS, XHCI_STS_HALTED, 0,
                           XHCI_INIT_TIMEOUT) != 0)
    {
        x->initialized = 0;
        return -2;
    }

    x->running = 1;
    xhci_memory_barrier();
    return 0;
}

int xhci_init(pci_device_t *dev)
{
    if (!dev)
        return -1;
    if (dev->class_code != 0x0C || dev->subclass_code != 0x03 || dev->prog_if != 0x30)
        return -2;

    memset(&g_xhci_controller, 0, sizeof(g_xhci_controller));
    xhci_controller_t *x = &g_xhci_controller;
    x->pci = dev;
    spin_lock_init(&x->lock);

    pci_enable_mmio_busmastering(dev);

    uint64_t physical = 0;
    if (xhci_get_mmio_base(dev, &physical) != 0)
        return -3;
    x->mmio_phys = (uintptr_t)physical;
    x->mmio = (volatile uint8_t *)vmm_map_device((uintptr_t)physical, XHCI_MAP_SIZE);
    if (!x->mmio)
        return -4;

    if (xhci_read_capabilities(x) != 0 || xhci_validate_mmio_layout(x) != 0)
        return -5;

    x->op = x->mmio + x->caps.cap_length;
    x->runtime = x->mmio + x->caps.rtsoff;
    x->doorbells = x->mmio + x->caps.dboff;

    if (xhci_legacy_handoff(x) != 0)
    {
        kprintf("[xHCI] Falha no BIOS/OS Ownership Handoff\n");
        return -6;
    }
    uint32_t page_size = xhci_read32(x->op, XHCI_PAGESIZE);
    x->caps.pagesize = page_size;
    if (!(page_size & 1u))
    {
        kprintf("[xHCI] Controlador nao suporta paginas de 4 KB\n");
        return -7;
    }
    if (xhci_reset_controller(x) != 0)
    {
        kprintf("[xHCI] Falha ao reiniciar controlador\n");
        return -8;
    }
    if (xhci_setup_dma(x) != 0)
    {
        kprintf("[xHCI] Falha ao configurar estruturas DMA/scratchpad\n");
        return -9;
    }
    if (xhci_start_controller(x) != 0)
    {
        kprintf("[xHCI] Falha ao iniciar controlador\n");
        return -10;
    }

    if (xhci_test_dma(x) != 0)
    {
        kprintf("[xHCI] Teste DMA falhou\n");
    }

    kprintf("[xHCI] Versao %x, Slots: %u, Portas: %u, Contexto: %u bytes, Scratchpads: %u\n",
            x->caps.version, x->caps.max_slots, x->caps.max_ports,
            x->caps.context_size, x->caps.scratchpad_count);
    /* Drena eventos iniciais (ex: Port Status Change gerados no arranque) */
    xhci_process_events(x);
    memset((void *)x->port_change_pending, 0, sizeof(x->port_change_pending));
    xhci_scan_ports(x);
    return 0;
}