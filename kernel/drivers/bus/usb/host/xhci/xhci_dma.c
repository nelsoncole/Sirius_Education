
#include <kernel/drivers/bus/usb/host/xhci/xhci_dma.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096UL
#endif

static int xhci_dma_round_size(size_t size, size_t *rounded)
{
    if (!size || !rounded)
        return -1;

    if (size > (size_t)-1 - (PAGE_SIZE - 1))
        return -2;

    *rounded = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return 0;
}

int xhci_dma_alloc(xhci_dma_t *dma, size_t size)
{
    if (!dma)
        return -1;

    size_t allocation_size;

    if (xhci_dma_round_size(size, &allocation_size) != 0)
        return -2;

    dma->virt = NULL;
    dma->phys = 0;
    dma->size = 0;

    void *virt = pool_alloc(allocation_size);

    if (!virt)
        return -3;

    uintptr_t phys = vmm_get_physical((uintptr_t)virt);

    if (!phys)
    {
        pool_free(virt, allocation_size);
        return -4;
    }

    memset(virt, 0, allocation_size);

    dma->virt = virt;
    dma->phys = (uint64_t)phys;
    dma->size = allocation_size;

    return 0;
}

void xhci_dma_free(xhci_dma_t *dma)
{
    if (!dma)
        return;

    if (dma->virt && dma->size)
        pool_free(dma->virt, dma->size);

    dma->virt = NULL;
    dma->phys = 0;
    dma->size = 0;
}