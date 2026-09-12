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
 *  Modified Date: 30/08/2026
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
    pmm_bitmap[page_index / 8] |= (1ULL << (page_index % 8));
}

static inline void bitmap_clear_bit(unsigned long page_index) {
    pmm_bitmap[page_index / 8] &= ~(1ULL << (page_index % 8));
}

void pmm_init(BOOT_INFO *boot_info) {
    MEMORY_MAP_INFO *map = &boot_info->MemoryMap;

    if(map->MemoryRegionCount < 1) {
        kprintf("[PMM ERRO CRITICO] Nao foi encontrada nenhuma regiao de memoria!\n");
        for(;;);
    }

    // Converte a memória alocada pelo bootloader UEFI em RAM livre para o kernel utilizar
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        if (map->MemoryRegions[i].Type == MEMORY_LOADER_DATA) {
            map->MemoryRegions[i].Type = MEMORY_FREE;
        }
    }
    
    // 1. Encontrar o endereço físico absoluto mais alto
    unsigned long highest_physical_address = 0;
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        unsigned long region_end = map->MemoryRegions[i].Start + map->MemoryRegions[i].Size;
        if (region_end > highest_physical_address) {
            highest_physical_address = region_end;
        }
    }

    // Calcula o total de páginas reais de ponta a ponta
    pmm_total_pages = highest_physical_address / PAGE_SIZE;
    
    // Cada byte do bitmap controla 8 páginas
    pmm_bitmap_size = pmm_total_pages / 8;
    if (pmm_total_pages % 8) pmm_bitmap_size++;

    kprintf("[PMM] Total de RAM: %d MB (%d paginas de 4KB).\n", map->InstalledRAM / 1024 / 1024, pmm_total_pages);
    kprintf("[PMM] Tamanho do Bitmap necessario: %d bytes.\n", pmm_bitmap_size);

    // 2. Encontrar um local seguro para colocar o Bitmap (Evitando Overlap com o Kernel)
    unsigned long bitmap_phys_addr = 0;
    unsigned long kernel_end = boot_info->KernelAddress + boot_info->KernelMemorySize;
    
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        MEMORY_REGION reg = map->MemoryRegions[i];
        unsigned long reg_end = reg.Start + reg.Size;
        
        if (reg.Type == MEMORY_FREE && reg.Size >= pmm_bitmap_size) {
            // CORREÇÃO 1: Validação de intervalo completo. O bitmap e o kernel não podem cruzar-se!
            if (!(reg.Start < kernel_end && reg_end > boot_info->KernelAddress)) {
                bitmap_phys_addr = reg.Start;
                break;
            }
        }
    }

    if (bitmap_phys_addr == 0) {
        kprintf("[PMM ERRO CRITICO] Nao foi possivel alocar espaco para o Bitmap do PMM!\n");
        for(;;);
    }

    // Mapeia o bitmap na árvore de páginas virtuais
    unsigned long bitmap_virt_addr = paging_map_region_bitmap(boot_info, bitmap_phys_addr, pmm_bitmap_size);
    pmm_bitmap = (unsigned char *) bitmap_virt_addr;
    kprintf("[PMM] Bitmap alocado no endereco fisico: 0x%lX mapeado em %p\n", bitmap_phys_addr, pmm_bitmap);
    
    // 3. Inicializar o Bitmap completo como "Ocupado" (Prevenção segura)
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    
    // 4. Mapear o estado real da RAM Livre
    for (unsigned long i = 0; i < map->MemoryRegionCount; i++) {
        MEMORY_REGION reg = map->MemoryRegions[i];
        
        if (reg.Type == MEMORY_FREE) {
            unsigned long start_page = reg.Start / PAGE_SIZE;
            unsigned long num_pages = reg.Size / PAGE_SIZE;
            
            for (unsigned long p = 0; p < num_pages; p++) {
                bitmap_clear_bit(start_page + p); // 0 = Livre
            }
        }
    }

    // 5. RESERVAS DE SEGURANÇA (Subscreve o Passo 4 garantindo isolamento)
    // Reserva o Kernel
    unsigned long kernel_start_page = boot_info->KernelAddress / PAGE_SIZE;
    unsigned long kernel_num_pages = (boot_info->KernelMemorySize + PAGE_SIZE - 1) / PAGE_SIZE;

    for (unsigned long p = 0; p < kernel_num_pages; p++) {
        bitmap_set_bit(kernel_start_page + p);
    }

    // CORREÇÃO 2: Força a proteção estrita das páginas do próprio bitmap.
    // Garante que o Passo 4 não deixou o endereço do bitmap marcado como livre por engano.
    unsigned long bitmap_start_page = bitmap_phys_addr / PAGE_SIZE;
    unsigned long bitmap_num_pages = (pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

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
        if (pmm_bitmap[i] == 0xFF) {
            continue;
        }

        // Se o byte tem pelo menos um bit em 0, descobrimos qual é
        for (int bit = 0; bit < 8; bit++)
        {
            // CORREÇÃO 1: Usa 1ULL (64-bit Unsigned Long Long) para evitar Sign Extension e lixo de 32-bit
            if (!(pmm_bitmap[i] & (1ULL << bit)))
            {
                // Calcula o índice global da página na RAM
                unsigned long page_index = (i * 8) + bit;

                // Proteção de segurança contra estouro do limite físico da RAM detectada
                if (page_index >= pmm_total_pages) {
                    return 0; 
                }

                // Aloca a página marcando o bit como 1 (Ocupado)
                bitmap_set_bit(page_index);

                // CORREÇÃO 2: Garante que a multiplicação usa aritmética estrita de 64 bits (PAGE_SIZE forçado a ULL)
                unsigned long phys_address = page_index * (unsigned long)PAGE_SIZE;
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

    // Liberta a página no bitmap resetando o bit correspondente
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
