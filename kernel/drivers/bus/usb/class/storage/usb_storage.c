/*
 * ==========================================================================
 * Project: Sirius_Education
 * Filename: usb_storage.c
 * Description: USB Mass Storage BOT/SCSI e integração com block_device_t.
 *              Todas as transferências passam por um buffer DMA permanente.
 * Author: Nelson Cole
 * License: MIT
 * ==========================================================================
 */
#include <kernel/drivers/bus/usb/class/storage/usb_storage.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>

#define BOT_CBW_SIG 0x43425355u
#define BOT_CSW_SIG 0x53425355u
#define BOT_CBW_SIZE 31U
#define BOT_CSW_SIZE 13U
#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_INQUIRY 0x12
#define SCSI_READ_CAPACITY10 0x25
#define SCSI_READ10 0x28
#define SCSI_WRITE10 0x2A
#define SCSI_INQUIRY_SIZE 36U
#define SCSI_CAPACITY_SIZE 8U

typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t tag;
    uint32_t transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t cdb_length;
    uint8_t cdb[16];
} bot_cbw_t;

typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} bot_csw_t;

static void usb_storage_lock(usb_storage_device_t *u)
{
    while (__sync_lock_test_and_set(&u->io_lock, 1U)) {
        while (u->io_lock)
            __builtin_ia32_pause();
    }
}

static void usb_storage_unlock(usb_storage_device_t *u)
{
    __sync_lock_release(&u->io_lock);
}

static uint32_t usb_storage_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int usb_storage_dma_init(usb_storage_device_t *u)
{
    if (!u || !u->bulk_transfer)
        return -1;

    if (u->dma_initialized)
        return (u->dma.virt && u->dma.phys &&
                u->dma.size >= USB_STORAGE_DMA_SIZE) ? 0 : -2;

    if (xhci_dma_alloc(&u->dma, USB_STORAGE_DMA_SIZE) != 0)
        return -3;

    if (!u->dma.virt || !u->dma.phys ||
        u->dma.size < USB_STORAGE_DMA_SIZE)
        return -4;

    u->io_lock = 0;
    u->dma_initialized = 1;
    return 0;
}

/* Executa CBW, fase de dados e CSW. direction_in=1 significa USB IN. */
static int usb_storage_bot_command(usb_storage_device_t *u,
                                   const uint8_t *cdb, uint8_t cdb_length,
                                   void *buffer, uint32_t length,
                                   int direction_in)
{
    if (!u || !cdb || !cdb_length || cdb_length > 16 ||
        (length && !buffer))
        return -1;
    if (usb_storage_dma_init(u) != 0)
        return -2;
    if (!u->endpoint_in || !u->endpoint_out)
        return -3;

    usb_storage_lock(u);

    bot_cbw_t cbw;
    bot_csw_t csw;
    memset(&cbw, 0, sizeof(cbw));
    memset(&csw, 0, sizeof(csw));

    uint32_t tag = ++u->next_tag;
    if (!tag)
        tag = ++u->next_tag;

    cbw.signature = BOT_CBW_SIG;
    cbw.tag = tag;
    cbw.transfer_length = length;
    cbw.flags = direction_in ? 0x80 : 0;
    cbw.lun = u->lun;
    cbw.cdb_length = cdb_length;
    memcpy(cbw.cdb, cdb, cdb_length);
    memcpy(u->dma.virt, &cbw, BOT_CBW_SIZE);

    if (u->bulk_transfer(u->transport_context, u->endpoint_out,
                         u->dma.virt, BOT_CBW_SIZE, 0) != 0) {
        usb_storage_unlock(u);
        return -4;
    }

    uint32_t offset = 0;
    while (offset < length) {
        uint32_t chunk = length - offset;
        if (chunk > USB_STORAGE_DMA_SIZE)
            chunk = USB_STORAGE_DMA_SIZE;

        if (!direction_in)
            memcpy(u->dma.virt, (const uint8_t *)buffer + offset, chunk);

        if (u->bulk_transfer(u->transport_context,
                             direction_in ? u->endpoint_in : u->endpoint_out,
                             u->dma.virt, chunk, direction_in) != 0) {
            usb_storage_unlock(u);
            return -5;
        }

        if (direction_in)
            memcpy((uint8_t *)buffer + offset, u->dma.virt, chunk);
        offset += chunk;
    }

    memset(u->dma.virt, 0, BOT_CSW_SIZE);
    if (u->bulk_transfer(u->transport_context, u->endpoint_in,
                         u->dma.virt, BOT_CSW_SIZE, 1) != 0) {
        usb_storage_unlock(u);
        return -6;
    }

    memcpy(&csw, u->dma.virt, BOT_CSW_SIZE);
    usb_storage_unlock(u);

    if (csw.signature != BOT_CSW_SIG || csw.tag != tag)
        return -7;
    if (csw.residue > length)
        return -8;
    if (csw.status != 0 || csw.residue != 0)
        return -9;

    return 0;
}

int usb_storage_probe(usb_storage_device_t *u)
{
    if (!u || usb_storage_dma_init(u) != 0)
        return -1;

    uint8_t cdb[16];
    uint8_t inquiry[SCSI_INQUIRY_SIZE];
    uint8_t capacity[SCSI_CAPACITY_SIZE];

    memset(cdb, 0, sizeof(cdb));
    memset(inquiry, 0, sizeof(inquiry));
    cdb[0] = SCSI_INQUIRY;
    cdb[4] = SCSI_INQUIRY_SIZE;
    if (usb_storage_bot_command(u, cdb, 6, inquiry,
                                sizeof(inquiry), 1) != 0)
        return -2;

    /* A Peripheral Device Type 0x1F indica LUN/dispositivo não suportado. */
    if ((inquiry[0] & 0x1F) == 0x1F)
        return -3;

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_TEST_UNIT_READY;
    if (usb_storage_bot_command(u, cdb, 6, NULL, 0, 0) != 0)
        return -4;

    memset(cdb, 0, sizeof(cdb));
    memset(capacity, 0, sizeof(capacity));
    cdb[0] = SCSI_READ_CAPACITY10;
    if (usb_storage_bot_command(u, cdb, 10, capacity,
                                sizeof(capacity), 1) != 0)
        return -5;

    uint32_t last_lba = usb_storage_be32(&capacity[0]);
    uint32_t sector_size = usb_storage_be32(&capacity[4]);
    if (last_lba == 0xFFFFFFFFU || !sector_size)
        return -6; /* READ CAPACITY(16) ainda não implementado. */

    u->total_sectors = (uint64_t)last_lba + 1ULL;
    u->sector_size = sector_size;
    return 0;
}

static int usb_storage_rw10(usb_storage_device_t *u, uint64_t lba,
                            uint32_t count, void *buffer, int write)
{
    if (!u || !buffer || !count || !u->sector_size || !u->total_sectors)
        return -1;
    if (count > 0xFFFFU)
        return -2;
    if (lba > 0xFFFFFFFFULL ||
        lba + (uint64_t)count > u->total_sectors ||
        lba + (uint64_t)count - 1ULL > 0xFFFFFFFFULL)
        return -3;

    uint64_t bytes = (uint64_t)count * u->sector_size;
    if (bytes > 0xFFFFFFFFULL)
        return -4;

    uint8_t cdb[10];
    memset(cdb, 0, sizeof(cdb));
    cdb[0] = write ? SCSI_WRITE10 : SCSI_READ10;
    cdb[2] = (uint8_t)(lba >> 24);
    cdb[3] = (uint8_t)(lba >> 16);
    cdb[4] = (uint8_t)(lba >> 8);
    cdb[5] = (uint8_t)lba;
    cdb[7] = (uint8_t)(count >> 8);
    cdb[8] = (uint8_t)count;

    return usb_storage_bot_command(u, cdb, sizeof(cdb), buffer,
                                   (uint32_t)bytes, write ? 0 : 1);
}

int usb_storage_read(usb_storage_device_t *u, uint64_t lba,
                     uint32_t count, void *buffer)
{
    return usb_storage_rw10(u, lba, count, buffer, 0);
}

int usb_storage_write(usb_storage_device_t *u, uint64_t lba,
                      uint32_t count, void *buffer)
{
    return usb_storage_rw10(u, lba, count, buffer, 1);
}

static int usb_storage_block_read(struct block_device *block, uint64_t lba,
                                  uint32_t count, void *buffer)
{
    if (!block || !block->private_data || !buffer)
        return -1;
    return usb_storage_read((usb_storage_device_t *)block->private_data,
                            lba, count, buffer);
}

static int usb_storage_block_write(struct block_device *block, uint64_t lba,
                                   uint32_t count, void *buffer)
{
    if (!block || !block->private_data || !buffer)
        return -1;
    return usb_storage_write((usb_storage_device_t *)block->private_data,
                             lba, count, buffer);
}

int usb_storage_register_block(usb_storage_device_t *u)
{
    if (!u || u->block_registered)
        return -1;
    if (usb_storage_dma_init(u) != 0)
        return -2;

    /* Obtém a capacidade real por SCSI antes de publicar o dispositivo. */
    if (usb_storage_probe(u) != 0) {
        kprintf("[USB] Falha ao identificar a unidade Mass Storage %u\n",
                (unsigned)u->device_id);
        return -3;
    }

    block_device_t *block = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (!block)
        return -4;

    memset(block, 0, sizeof(block_device_t));
    ksprintf(block->name, "usb%u", (unsigned)u->device_id);
    block->total_sectors = u->total_sectors;
    block->sector_size = u->sector_size;
    block->read_blocks = usb_storage_block_read;
    block->write_blocks = usb_storage_block_write;
    block->ioctl = NULL;
    block->private_data = u;
    block->is_raw = 1;

    int id = register_block_device(block);
    if (id < 0) {
        kfree(block);
        return -5;
    }

    u->block_id = (uint32_t)id;
    u->block_registered = 1;

    kprintf("[USB] Dispositivo de bloco %s registado [ID: %d]\n"
            "      Sector size: %u bytes\n"
            "      Total sectors: %lu\n",
            block->name, id, (unsigned)block->sector_size,
            (unsigned long)block->total_sectors);

    return id;
}