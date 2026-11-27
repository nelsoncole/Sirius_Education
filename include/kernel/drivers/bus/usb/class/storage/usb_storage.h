/*
 * ==========================================================================
 * Project: Sirius_Education
 * Filename: usb_storage.h
 * Description: Interface USB Mass Storage BOT/SCSI e dispositivo de bloco.
 * Author: Nelson Cole
 * License: MIT
 * ==========================================================================
 */
#ifndef USB_STORAGE_H
#define USB_STORAGE_H

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>
#include <kernel/drivers/bus/usb/host/xhci/xhci_dma.h>

#define USB_STORAGE_DMA_SIZE (64U * 1024U)

/*
 * O callback recebe um buffer DMA fisicamente contiguo. Deve mapear o
 * endpoint USB address para o endpoint_id/ring xHCI e confirmar que
 * actual_length == length antes de devolver sucesso.
 */
typedef int (*usb_storage_bulk_fn)(
    void *transport_context,
    uint8_t endpoint,
    void *dma_buffer,
    uint32_t length,
    int direction_in
);

typedef struct usb_storage_device {
    uint8_t device_id;
    uint8_t lun;
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uint32_t sector_size;
    uint32_t next_tag;
    uint64_t total_sectors;
    void *transport_context;
    usb_storage_bulk_fn bulk_transfer;
    xhci_dma_t dma;
    volatile uint32_t io_lock;
    uint32_t block_id;
    uint8_t dma_initialized;
    uint8_t block_registered;
} usb_storage_device_t;

int usb_storage_probe(usb_storage_device_t *device);
int usb_storage_read(usb_storage_device_t *device, uint64_t lba,
                     uint32_t count, void *buffer);
int usb_storage_write(usb_storage_device_t *device, uint64_t lba,
                      uint32_t count, void *buffer);
int usb_storage_register_block(usb_storage_device_t *device);

#endif