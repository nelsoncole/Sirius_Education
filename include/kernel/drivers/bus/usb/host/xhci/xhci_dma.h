#ifndef _XHCI_DMA_H_
#define _XHCI_DMA_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

typedef struct
{
    void *virt;
    uint64_t phys;
    size_t size;
} xhci_dma_t;

/* A memoria DMA permanece reservada durante a vida do controlador. */
int xhci_dma_alloc(xhci_dma_t *dma, size_t size);

void xhci_dma_free(xhci_dma_t *dma);

#endif
