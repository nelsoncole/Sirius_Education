/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pmm.c
 *    Description: Implementação do Gestor de Memória Física utilizando
 *                 a estratégia de Bitmap de Páginas.
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/mm/pmm.h>
#include <kernel/lib/stdio.h>
#include <kernel/lib/string.h>
#include <kernel/kernel.h>

// Variáveis internas do PMM
static unsigned char *pmm_bitmap = 0;
static unsigned long pmm_total_pages = 0;
static unsigned long pmm_bitmap_size = 0;

/*
 * Funções auxiliares para manipulação de bits
 */
static inline void bitmap_set_bit(unsigned long page_index) {
    pmm_bitmap[page_index / 8] |= (1 << (page_index % 8));
}

static inline void bitmap_clear_bit(unsigned long page_index) {
    pmm_bitmap[page_index / 8] &= ~(1 << (page_index % 8));
}

void pmm_init(BOOT_INFO *boot_info) {
    MEMORY_MAP_INFO *map = &boot_info->MemoryMap;
    
    // 1. Calcular o total de páginas com base na RAM Instalada
    pmm_total_pages = map->InstalledRAM / PAGE_SIZE;
    
    // Cada byte do bitmap controla 8 páginas.
    pmm_bitmap_size = pmm_total_pages / 8;
    if (pmm_total_pages % 8) pmm_bitmap_size++;

    kprintf("[PMM] Total de RAM: %d MB (%d paginas de 4KB).\n", map->InstalledRAM / 1024 / 1024, pmm_total_pages);
    kprintf("[PMM] Tamanho do Bitmap necessario: %d bytes.\n", pmm_bitmap_size);

    // 2. Encontrar um local seguro para colocar o Bitmap
    // Precisamos de uma região MEMORY_FREE grande o suficiente para o pmm_bitmap_size
    unsigned long bitmap_phys_addr = 0;
    
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        MEMORY_REGION reg = map->MemoryRegions[i];
        
        if (reg.Type == MEMORY_FREE && reg.Size >= pmm_bitmap_size) {
            // Garante que não colide com o endereço do próprio Kernel.
            // O bootloader UEFI já subtraiu e marcou a região da memória do kernel
            // como reservada (removendo-a de MEMORY_FREE)
            if (!(boot_info->KernelAddress >= reg.Start && 
                  boot_info->KernelAddress < (reg.Start + reg.Size))) {
                bitmap_phys_addr = reg.Start;
                break;
            }
        }
    }

    if (bitmap_phys_addr == 0) {
        kprintf("[PMM ERRO CRITICO] Nao foi possivel alocar espaco para o Bitmap do PMM!\n");
        for(;;);
    }

    /*
     * O mapeador agora recebe o tamanho total da bytes (pmm_bitmap_size)
     * e retorna o endereço virtual mapeado na árvore de páginas x86_64.
     */
    unsigned long bitmap_virt_addr = paging_map_region_bitmap(boot_info, bitmap_phys_addr, pmm_bitmap_size);

    // Atribuição direta através do retorno da função
    pmm_bitmap = (unsigned char *) bitmap_virt_addr;
    kprintf("[PMM] Bitmap alocado no endereco fisico: 0x%x mapeado em %p\n", bitmap_phys_addr, pmm_bitmap);

    // 3. Inicializar o Bitmap completo como "Ocupado" (Prevenção por segurança)
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);

    // 4. Mapear o estado real da RAM com base nas regiões do Bootloader
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        MEMORY_REGION reg = map->MemoryRegions[i];
        
        // Se a região for explicitamente utilizável (Livre)
        if (reg.Type == MEMORY_FREE) {
            unsigned long start_page = reg.Start / PAGE_SIZE;
            unsigned long num_pages = reg.Size / PAGE_SIZE;
            
            for (unsigned long p = 0; p < num_pages; p++) {
                bitmap_clear_bit(start_page + p); // 0 = Livre
            }
        }
    }

    // 5. Reservar as páginas onde o próprio Kernel e o Bitmap estão localizados
    unsigned long kernel_start_page = boot_info->KernelAddress / PAGE_SIZE;
    unsigned long kernel_num_pages = boot_info->KernelMemorySize / PAGE_SIZE;
    if (boot_info->KernelMemorySize % PAGE_SIZE) kernel_num_pages++;

    for (unsigned long p = 0; p < kernel_num_pages; p++) {
        bitmap_set_bit(kernel_start_page + p);
    }

    unsigned long bitmap_start_page = bitmap_phys_addr / PAGE_SIZE;
    unsigned long bitmap_num_pages = pmm_bitmap_size / PAGE_SIZE;
    if (pmm_bitmap_size % PAGE_SIZE) bitmap_num_pages++;

    for (unsigned long p = 0; p < bitmap_num_pages; p++) {
        bitmap_set_bit(bitmap_start_page + p);
    }

    kprintf("[PMM] Inicializacao concluida. Pronto para alocacao.\n");
}


/*
 * ============================================================================
 * Aloca uma única página de memória física livre (4 KB).
 * Varre o bitmap à procura do primeiro bit em 0, marca-o como 1 e retorna
 * o endereço físico correspondente. Retorna 0 se a memória estiver esgotada.
 * ============================================================================
 */
unsigned long pmm_alloc_page(void)
{
    // Varre o bitmap byte a byte
    for (unsigned long i = 0; i < pmm_bitmap_size; i++)
    {
        // Se o byte for 0xFF, significa que as 8 páginas deste bloco estão ocupadas.
        // Saltamos o byte imediatamente para acelerar a busca por hardware.
        if (pmm_bitmap[i] == 0xFF) {
            continue;
        }

        // Se o byte tem pelo menos um bit em 0, descobrimos qual é
        for (int bit = 0; bit < 8; bit++)
        {
            // Verifica se o bit atual está livre (0)
            if (!(pmm_bitmap[i] & (1 << bit)))
            {
                // Calcula o índice global da página na RAM
                unsigned long page_index = (i * 8) + bit;

                // Proteção de segurança contra estouro do limite físico da RAM detectada
                if (page_index >= pmm_total_pages) {
                    return 0; 
                }

                // Aloca a página marcando o bit como 1 (Ocupado)
                bitmap_set_bit(page_index);

                // Converte o índice da página de volta para o endereço físico real (page * 4KB)
                unsigned long phys_address = page_index * PAGE_SIZE;
                return phys_address;
            }
        }
    }

    kprintf("[PMM ERRO] Memoria fIsica esgotada! ImpossIvel alocar pagina.\n");
    return 0; // Out of memory
}

/*
 * ============================================================================
 * Liberta uma página de memória física previamente alocada.
 * Recebe o endereço físico (múltiplo de 4 KB) e reseta o seu bit para 0.
 * ============================================================================
 */
void pmm_free_page(unsigned long phys_address)
{
    // Proteção básica: garante que o endereço está alinhado à fronteira de 4 KB
    // e não tenta libertar o endereço zero por engano.
    if (phys_address % PAGE_SIZE != 0 || phys_address == 0) {
        return;
    }

    // Converte o endereço físico no índice linear da página
    unsigned long page_index = phys_address / PAGE_SIZE;

    // Proteção estrita para não tentar mexer fora dos limites da RAM do sistema
    if (page_index >= pmm_total_pages) {
        return;
    }

    // Liberta a página no bitmap resetando o bit correspondente para 0
    bitmap_clear_bit(page_index);
}


/*
 * ============================================================================
 * Aloca uma sequência contIgua de 'count' páginas de memória física (4 KB cada).
 * Varre o bitmap à procura do primeiro bloco de bits em 0 consecutivos.
 * Retorna o endereço físico do início do bloco ou 0 em caso de falha.
 * ============================================================================
 */
unsigned long pmm_alloc_pages(unsigned long count)
{
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    unsigned long consecutive_free = 0;
    unsigned long start_page = 0;

    // Varre todas as páginas gerenciadas pelo bitmap
    for (unsigned long i = 0; i < pmm_total_pages; i++)
    {
        // Verifica se a página atual está livre (bit em 0)
        if (!(pmm_bitmap[i / 8] & (1 << (i % 8))))
        {
            if (consecutive_free == 0) {
                start_page = i; // Regista onde o bloco livre começou
            }
            consecutive_free++;

            // Se encontrarmos a quantidade exata de páginas seguidas que precisamos
            if (consecutive_free == count)
            {
                // Marca todas as páginas deste bloco como ocupadas (1)
                for (unsigned long p = 0; p < count; p++) {
                    bitmap_set_bit(start_page + p);
                }
                
                // Retorna o endereço físico de partida (primeira página do bloco)
                return start_page * PAGE_SIZE;
            }
        }
        else
        {
            // Se encontrar uma única página ocupada a meio da busca,
            // quebra a sequência e reinicia o contador
            consecutive_free = 0;
        }
    }

    kprintf("[PMM ERRO] MemOria insuficiente para alocar %du paginas contIguas.\n", count);
    return 0; // Falha por fragmentação ou falta de memória RAM
}

/*
 * ============================================================================
 * Liberta uma sequência contIgua de 'count' páginas de memória física.
 * Recebe o endereço físico inicial do bloco e reseta os respetivos bits para 0.
 * ============================================================================
 */
void pmm_free_pages(unsigned long phys_address, unsigned long count)
{
    if (count == 0) return;
    if (count == 1) {
        pmm_free_page(phys_address);
        return;
    }

    // Proteção básica: o endereço deve estar alinhado a 4 KB
    if (phys_address % PAGE_SIZE != 0 || phys_address == 0) {
        return;
    }

    // Calcula o índice inicial da página
    unsigned long start_page = phys_address / PAGE_SIZE;

    // Proteção estrita contra estouro de limite de hardware
    if ((start_page + count) > pmm_total_pages) {
        return;
    }

    // Varre o bloco marcando todos os bits de volta para 0 (Livre)
    for (unsigned long p = 0; p < count; p++) {
        bitmap_clear_bit(start_page + p);
    }
}
