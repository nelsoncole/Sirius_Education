#include <kernel/drivers/bus/usb/host/xhci/xhci.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_regs.h>
#include <kernel/klib.h>

#define XHCI_COMMAND_TIMEOUT       10000000U
#define XHCI_COMMAND_SLOT_SHIFT    24
#define XHCI_ADDRESS_BSR           (1U << 9)
#define XHCI_CONFIG_DECONFIGURE    (1U << 9)

static inline void xhci_command_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static int xhci_command_validate(xhci_controller_t *x)
{
    if (!x || !x->initialized || !x->running)
        return -1;

    if (!x->command_ring.initialized)
        return -2;

    if (!x->doorbells)
        return -3;

    if (!x->caps.max_slots)
        return -4;

    return 0;
}

static int xhci_command_validate_context(
    xhci_controller_t *x,
    uint64_t input_context
)
{
    if (!x || !input_context)
        return -1;

    if (input_context & 0x3FULL)
        return -2;

    if (!x->caps.address_64bit && (input_context >> 32))
        return -3;

    if (x->caps.context_size != 32 && x->caps.context_size != 64)
        return -4;

    return 0;
}

static int xhci_command_execute(
    xhci_controller_t *x,
    uint8_t type,
    uint64_t parameter,
    uint32_t status,
    uint32_t control,
    uint8_t expected_slot,
    uint8_t *out_slot,
    uint8_t *completion_code
)
{
    int result = xhci_command_validate(x);

    if (result != 0)
        return result;

    xhci_trb_t command;
    memset(&command, 0, sizeof(command));

    command.parameter = parameter;
    command.status = status;
    command.control =
        ((uint32_t)type << XHCI_TRB_TYPE_SHIFT) | control;

    uint8_t returned_slot = 0;
    uint8_t returned_code = 0;

    xhci_command_barrier();

    result = xhci_command_submit(
        x,
        &command,
        XHCI_COMMAND_TIMEOUT,
        &returned_slot,
        &returned_code
    );

    if (out_slot)
        *out_slot = returned_slot;

    if (completion_code)
        *completion_code = returned_code;

    if (result != 0)
    {
        kprintf(
            "[xHCI] Comando %u falhou: resultado=%d completion=%u\n",
            type,
            result,
            returned_code
        );

        return result;
    }

    if (expected_slot && returned_slot != expected_slot)
    {
        kprintf(
            "[xHCI] Slot inesperado: esperado=%u recebido=%u\n",
            expected_slot,
            returned_slot
        );

        return -2;
    }

    return 0;
}

int xhci_enable_slot(xhci_controller_t *x, uint8_t *slot_id)
{
    if (!x || !slot_id)
        return -1;

    *slot_id = 0;
    uint8_t slot = 0;

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_ENABLE_SLOT,
        0,
        0,
        0,
        0,
        &slot,
        NULL
    );

    if (result != 0)
        return result;

    if (!slot || slot > x->caps.max_slots)
        return -2;

    *slot_id = slot;

    kprintf("[xHCI] Slot %u activado\n", slot);

    return 0;
}

int xhci_disable_slot(xhci_controller_t *x, uint8_t slot_id)
{
    if (!x || !slot_id || slot_id > x->caps.max_slots)
        return -1;

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_DISABLE_SLOT,
        0,
        0,
        (uint32_t)slot_id << XHCI_COMMAND_SLOT_SHIFT,
        slot_id,
        NULL,
        NULL
    );

    if (result == 0)
        kprintf("[xHCI] Slot %u desactivado\n", slot_id);

    return result;
}

int xhci_address_device(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint64_t input_context,
    uint8_t block_set_address
)
{
    if (!x || !slot_id || slot_id > x->caps.max_slots)
        return -1;

    if (xhci_command_validate_context(x, input_context) != 0)
        return -2;

    uint32_t control =
        (uint32_t)slot_id << XHCI_COMMAND_SLOT_SHIFT;

    if (block_set_address)
        control |= XHCI_ADDRESS_BSR;

    kprintf(
        "[xHCI DEBUG] ADDRESS DEVICE: slot=%u input_context=0x%llx BSR=%u\n",
        slot_id,
        (unsigned long long)input_context,
        block_set_address
    );

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_ADDRESS_DEVICE,
        input_context,
        0,
        control,
        slot_id,
        NULL,
        NULL
    );

    if (result == 0)
    {
        kprintf(
            "[xHCI] Address Device concluido para slot %u\n",
            slot_id
        );
    }

    return result;
}

int xhci_configure_endpoint(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint64_t input_context,
    uint8_t deconfigure
)
{
    if (!x || !slot_id || slot_id > x->caps.max_slots)
        return -1;

    /* Na especificação xHCI, se Deconfigure (DC=1) estiver ativo, input_context é ignorado (pode ser 0) */
    if (!deconfigure && xhci_command_validate_context(x, input_context) != 0)
        return -2;

    uint32_t control =
        (uint32_t)slot_id << XHCI_COMMAND_SLOT_SHIFT;

    if (deconfigure)
        control |= XHCI_CONFIG_DECONFIGURE;

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,
        deconfigure ? 0 : input_context,
        0,
        control,
        slot_id,
        NULL,
        NULL
    );

    if (result == 0)
        kprintf("[xHCI] Configure Endpoint concluido para slot %u\n", slot_id);

    return result;
}

int xhci_evaluate_context(
    xhci_controller_t *x,
    uint8_t slot_id,
    uint64_t input_context
)
{
    if (!x || !slot_id || slot_id > x->caps.max_slots)
        return -1;

    if (xhci_command_validate_context(x, input_context) != 0)
        return -2;

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_EVALUATE_CONTEXT,
        input_context,
        0,
        (uint32_t)slot_id << XHCI_COMMAND_SLOT_SHIFT,
        slot_id,
        NULL,
        NULL
    );

    if (result == 0)
        kprintf("[xHCI] Evaluate Context concluido para slot %u\n", slot_id);

    return result;
}


int xhci_test_dma(xhci_controller_t *x)
{
    if (!x || !x->running || !x->initialized)
        return -1;

    kprintf("[xHCI DMA TEST] Inicio\n");

    uint8_t slot = 0;
    uint8_t completion = 0;

    int result = xhci_command_execute(
        x,
        XHCI_TRB_TYPE_NOOP_COMMAND,
        0,
        0,
        0,
        0,
        &slot,
        &completion
    );

    if (result != 0)
    {
        kprintf(
            "[xHCI DMA TEST] FALHOU: result=%d CC=%u\n",
            result,
            completion
        );

        return -2;
    }

    kprintf(
        "[xHCI DMA TEST] SUCESSO: Command Ring e Event Ring operacionais\n"
    );

    return 0;
}