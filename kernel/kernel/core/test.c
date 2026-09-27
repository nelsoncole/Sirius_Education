/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: test.c
 *    Description: Demonstração de leitura de setores em bloco via DMA AHCI
 *                 focado na validação e extração de metadados avançados de
 *                 discos com estrutura GPT (LBA 1).
 * ============================================================================
 */

#include <kernel/klib.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/drivers/storage/partitions.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/kvmm.h>


void test(void* arg) 
{
    (void)arg;
    kprintf("[BOOT] A varrer dispositivos de armazenamento à procura de partições...\n");

    block_list_devices();
    while (1) {
        __asm__ __volatile__("hlt");
    }
}