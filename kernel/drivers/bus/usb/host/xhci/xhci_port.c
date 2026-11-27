
#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>

#define XHCI_PORT_PP       (1u << 9)
#define XHCI_PORT_WCE      (1u << 25)
#define XHCI_PORT_WDE      (1u << 26)
#define XHCI_PORT_WOE      (1u << 27)

#define XHCI_PORT_RW_PRESERVE \
    (XHCI_PORT_PP | XHCI_PORT_WCE | \
     XHCI_PORT_WDE | XHCI_PORT_WOE)

#define XHCI_PORT_TIMEOUT 10000000u

static inline uint32_t xhci_port_read32(
    volatile uint8_t *base,
    uint32_t offset
)
{
    return *(volatile uint32_t *)(base + offset);
}

static inline void xhci_port_write32(
    volatile uint8_t *base,
    uint32_t offset,
    uint32_t value
)
{
    *(volatile uint32_t *)(base + offset) = value;
}

static inline void xhci_port_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static volatile uint8_t *xhci_port_register(
    xhci_controller_t *x,
    uint8_t port_id
)
{
    return x->op + XHCI_PORTSC_BASE +
           ((uint32_t)port_id - 1u) * XHCI_PORT_STRIDE;
}

/*
 * Limpa apenas os bits de alteração W1C.
 * Não escreve novamente os campos RO/RW do PORTSC.
 */
static void xhci_port_clear_changes(volatile uint8_t *port)
{
    uint32_t value = xhci_port_read32(port, XHCI_PORTSC);

    uint32_t changes = value & XHCI_PORT_CHANGE_BITS;

    if (!changes)
        return;

    uint32_t write_value =
        (value & XHCI_PORT_RW_PRESERVE) | changes;

    xhci_port_write32(port, XHCI_PORTSC, write_value);

    xhci_port_barrier();
}

static int xhci_port_wait_reset(
    xhci_controller_t *x,
    uint8_t port_id,
    uint32_t reset_bit
)
{
    volatile uint8_t *port = xhci_port_register(x, port_id);

    for (uint32_t i = 0; i < XHCI_PORT_TIMEOUT; i++)
    {
        uint32_t value = xhci_port_read32(port, XHCI_PORTSC);

        if (!(value & XHCI_PORT_CCS))
            return -1;

        if (!(value & reset_bit) && (value & XHCI_PORT_PED))
            return 0;

        __builtin_ia32_pause();
    }

    return -2;
}

int xhci_reset_port(xhci_controller_t *x, uint8_t port_id)
{
    if (!x || !x->op || !port_id ||
        port_id > x->caps.max_ports)
        return -1;

    volatile uint8_t *port = xhci_port_register(x, port_id);

    uint32_t value = xhci_port_read32(port, XHCI_PORTSC);

    if (!(value & XHCI_PORT_CCS))
        return -2;

    uint8_t speed = (uint8_t)XHCI_PORT_SPEED(value);

    uint32_t reset_bit =
        (speed >= 4u) ? XHCI_PORT_WPR : XHCI_PORT_PR;

    /* Remove alterações pendentes antes do reset. */
    xhci_port_clear_changes(port);

    /*
     * Escreve exclusivamente o comando de reset.
     * Não reutiliza o valor lido do PORTSC.
     */
    value = xhci_port_read32(port, XHCI_PORTSC);

    uint32_t write_value =
        (value & XHCI_PORT_RW_PRESERVE) | reset_bit;

    xhci_port_write32(port, XHCI_PORTSC, write_value);
    xhci_port_barrier();

    int result = xhci_port_wait_reset(x, port_id, reset_bit);

    value = xhci_port_read32(port, XHCI_PORTSC);

    kprintf("[xHCI] PORTSC apos reset: %08x PP=%u PED=%u SPEED=%u\n",
        value,
        (value >> 9) & 1,
        (value >> 1) & 1,
        XHCI_PORT_SPEED(value));

    if (result != 0)
    {
        uint32_t current = xhci_port_read32(port, XHCI_PORTSC);

        kprintf(
            "[xHCI] Reset falhou: porta=%u speed=%u PORTSC=%x erro=%d\n",
            port_id,
            speed,
            current,
            result
        );

        return -3;
    }

    xhci_port_barrier();

    /* Reconhece os eventos produzidos pelo reset. */
    xhci_port_clear_changes(port);

    value = xhci_port_read32(port, XHCI_PORTSC);

    if (!(value & XHCI_PORT_PED))
        return -4;

    kprintf(
        "[xHCI] Porta %u pronta: speed=%u PORTSC=%x\n",
        port_id,
        (unsigned)XHCI_PORT_SPEED(value),
        value
    );

    return 0;
}

static int xhci_find_device_on_port(
    xhci_controller_t *x,
    uint8_t port_id
)
{
    for (uint16_t slot = 1; slot < XHCI_MAX_DEVICES; slot++)
    {
        if (x->devices[slot].connected &&
            x->devices[slot].port_id == port_id)
            return (int)slot;
    }

    return -1;
}

void xhci_scan_ports(xhci_controller_t *x)
{
    if (!x || !x->op || !x->running)
        return;

    for (uint16_t p = 1; p <= x->caps.max_ports; p++)
    {
        volatile uint8_t *port =
            xhci_port_register(x, (uint8_t)p);

        uint32_t value = xhci_port_read32(port, XHCI_PORTSC);

        if (!(value & XHCI_PORT_CCS))
            continue;

        if (xhci_reset_port(x, (uint8_t)p) != 0)
            continue;

        /*
         * A velocidade deve ser obtida novamente depois do reset.
         */
        value = xhci_port_read32(port, XHCI_PORTSC);

        if (!(value & XHCI_PORT_PED))
            continue;

        uint8_t speed = (uint8_t)XHCI_PORT_SPEED(value);

        (void)xhci_enumerate_device(x, (uint8_t)p, speed);
    }
}

void xhci_process_port_changes(xhci_controller_t *x)
{
    if (!x || !x->op || !x->running)
        return;

    for (uint32_t group = 0; group < 8; group++)
    {
        uint32_t pending = __atomic_exchange_n(
            &x->port_change_pending[group],
            0,
            __ATOMIC_ACQ_REL
        );

        while (pending)
        {
            uint32_t bit_index = (uint32_t)__builtin_ctz(pending);

            pending &= pending - 1u;

            uint32_t port_number =
                group * 32u + bit_index + 1u;

            if (!port_number ||
                port_number > x->caps.max_ports)
                continue;

            uint8_t port_id = (uint8_t)port_number;

            volatile uint8_t *port =
                xhci_port_register(x, port_id);

            uint32_t value = xhci_port_read32(port, XHCI_PORTSC);

            /* Reconhece as alterações sem reescrever PED ou reset. */
            xhci_port_clear_changes(port);

            int existing = xhci_find_device_on_port(x, port_id);

            if (!(value & XHCI_PORT_CCS))
            {
                if (existing >= 0)
                    xhci_device_disconnect(x, (uint8_t)existing);

                continue;
            }

            if (existing >= 0)
                continue;

            if (xhci_reset_port(x, port_id) != 0)
                continue;

            value = xhci_port_read32(port, XHCI_PORTSC);

            if (!(value & XHCI_PORT_PED))
                continue;

            uint8_t speed = (uint8_t)XHCI_PORT_SPEED(value);

            (void)xhci_enumerate_device(x, port_id, speed);
        }
    }
}
