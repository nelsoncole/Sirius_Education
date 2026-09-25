/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: console.c
 *    Description: Thread de Kernel do Emulador de Tela Assíncrono.
 *                 Consome as filas de saída das TTYs dinamicamente de forma isolada
 *                 com suporte a Backbuffers Circulares de múltiplas páginas gráficos.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 15/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 23/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/dev/vfs_tty.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/drivers/video/video.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

extern void tty_putc_backbuffer(struct tty_device *tty, char c);
extern void *optimized_memcpy(void *dst, const void *src, size_t bytes);

int tty_ready;

/**
 * tty_flush_to_screen - Transfere a janela visível atual (Split Screen) para a VRAM.
 */
void tty_flush_to_screen(struct tty_device *tty) {
    if (!tty || !tty->video_buffer || !g_display.frame_buffer_base) return;

    unsigned int max_virtual_height = tty->height * tty->total_pages;
    
    // Cast seguro para manipulação linear e precisa de bytes brutos pelo compilador
    uint8_t *vram = (uint8_t*)g_display.frame_buffer_base;
    uint8_t *buffer_base = (uint8_t*)tty->video_buffer;
    
    // Bytes contidos numa única linha horizontal do ecrã gráfico
    uint32_t bytes_per_scanline = tty->pixels_per_scanLine * sizeof(uint32_t);

    /* Caso 1: A janela visível está linear dentro do buffer (Sem quebra circular) */
    if (tty->scroll_y_offset + tty->height <= max_virtual_height) {
        uint8_t *src = buffer_base + (tty->scroll_y_offset * bytes_per_scanline);
        uint32_t size_bytes = tty->height * bytes_per_scanline;
        
        optimized_memcpy(vram, src, size_bytes);
    } 
    /* Caso 2: A janela quebrou na borda circular física! Divide a cópia em duas partes */
    else {
        // Parte 1: Do offset visível até ao fim absoluto do buffer de memória
        unsigned int lines_part1 = max_virtual_height - tty->scroll_y_offset;
        uint32_t size_part1 = lines_part1 * bytes_per_scanline;
        uint8_t *src_part1 = buffer_base + (tty->scroll_y_offset * bytes_per_scanline);
        
        optimized_memcpy(vram, src_part1, size_part1);

        // Parte 2: Do início absoluto (0) do buffer até preencher o resto da altura da ecrã
        unsigned int lines_part2 = tty->height - lines_part1;
        uint32_t size_part2 = lines_part2 * bytes_per_scanline;
        uint8_t *vram_part2 = vram + size_part1; // Avança os bytes que já foram copiados
        
        optimized_memcpy(vram_part2, buffer_base, size_part2);
    }
}

/**
 * tty_emulator_thread - Ponto de entrada da Thread de Kernel do Emulador.
 *                       Roda em segundo plano consumindo e isolando todas as TTYs.
 */
void tty_emulator_thread() {
    char c;

    tty_ready = 1;
    vfs_node_t* last_active_node = NULL;

    /* LAÇO INFINITO DE EXECUÇÃO DA KTHREAD */
    while (1) {
        int teve_dados = 0;
        
        /* 1. CONSUMO GLOBAL PASSIVO: Esvazia as filas de todas as TTYs do sistema */
        for (int i = 0; i < MAX_TTY_DRV_DEVICES; i++) {
            vfs_node_t* node = tty_vfs_get_node_by_index(i);
            if (!node || !node->private_data) continue;

            struct tty_device* tty = (struct tty_device*)node->private_data;

            while (tty_pop_output(tty, &c)) {
                teve_dados = 1;
                tty_putc_backbuffer(tty, c);
            }
        }

        /* 2. REFRESH GRÁFICO ATÓMICO CONVENIENTE */
        vfs_node_t* active_node = tty_vfs_get_active_node();
        if (!active_node) {
            active_node = tty_vfs_get_node_by_index(0); // Fallback tty0
        }

        if (active_node && active_node->private_data) {
            struct tty_device* active_tty = (struct tty_device*)active_node->private_data;
            
            int mudou_foco = (active_node != last_active_node);
            int precisa_refresh = __sync_lock_test_and_set(&active_tty->refresh_needed, 0);

            if (precisa_refresh || mudou_foco) {
                tty_flush_to_screen(active_tty);
                // Garante a atualização imediata do ponteiro para travar loops infinitos
                last_active_node = active_node; 
            }
        }

        /* 3. POLÍTICA DE COOPERAÇÃO OBRIGATÓRIA (FIM DO CONGELAMENTO) */
        if (!teve_dados) {
            /* 
             * CORREÇÃO CRÍTICA: Não podes usar apenas "pause". Tens de ceder o CPU!
             * Use a função de yield (ex: scheduler_yield(), thread_yield()).
             * Isto permite que as threads do teclado e das aplicações Ring 3 rodem imediatamente.
             */
            scheduler_yield(); 
            __asm__ __volatile__("pause");
            
        }
    }
}