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
 *  Modified Date: 26/10/2026
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

void tty_flush_to_screen(struct tty_device *tty, int offset)
{
    if (!tty || !tty->video_buffer || !g_display.frame_buffer_base)
        return;

    if (g_display.bpp != 32)
        return;

    if (tty->width == 0 || tty->height == 0 ||
        tty->total_pages == 0 ||
        g_display.width == 0 || g_display.height == 0)
        return;

    if (offset < 0 || tty->width > g_display.width)
        return;

    size_t total_lines = (size_t)tty->height * tty->total_pages;
    size_t framebuffer_pitch = g_display.pitch;

    if (framebuffer_pitch < (size_t)g_display.width * sizeof(uint32_t))
        return;

    if ((size_t)offset > total_lines ||
        g_display.height > total_lines - (size_t)offset)
        return;

    size_t src_total_bytes =
        total_lines * (size_t)tty->width * sizeof(uint32_t);

    size_t dst_total_bytes =
        framebuffer_pitch * (size_t)g_display.height;

    if (src_total_bytes == 0 || dst_total_bytes == 0)
        return;

    const uint32_t *src_base = (const uint32_t *)tty->video_buffer;
    uint8_t *dst_base = (uint8_t *)g_display.frame_buffer_base;

    src_base += (size_t)offset * tty->width;

    size_t copy_bytes = (size_t)tty->width * sizeof(uint32_t);

    for (size_t y = 0; y < g_display.height; y++)
    {
        uint8_t *dst_row = dst_base + y * framebuffer_pitch;

        const uint32_t *src_row =
            src_base + y * (size_t)tty->width;

        optimized_memcpy(dst_row, src_row, copy_bytes);
    }
}


/**
 * tty_emulator_thread - Ponto de entrada da Thread de Kernel do Emulador.
 *                       Roda em segundo plano consumindo e isolando todas as TTYs.
 */
void tty_emulator_thread(void* arg) {
    (void)arg;
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
        
        struct tty_device* active_tty = (struct tty_device*)tty_get_current();
        
        int precisa_refresh = 0;
        int mudou_foco = (active_node != last_active_node);
        
        for (int i = 0; i < MAX_TTY_DRV_DEVICES; i++) {
            vfs_node_t* node = tty_vfs_get_node_by_index(i);
            if (!node || !node->private_data) continue;

            struct tty_device* tty = (struct tty_device*)node->private_data;
            precisa_refresh = __sync_lock_test_and_set(&tty->refresh_needed, 0);
            if ((precisa_refresh && tty == active_tty) || (mudou_foco && tty == active_tty)) {
                tty_flush_to_screen(active_tty, tty->scroll_y_offset);
                // Garante a atualização imediata do ponteiro para travar loops infinitos
                last_active_node = active_node; 
            }

        }

        /* 3. POLÍTICA DE COOPERAÇÃO OBRIGATÓRIA (FIM DO CONGELAMENTO) */
        if (!teve_dados && !precisa_refresh && !mudou_foco) {
            /* 
             * CORREÇÃO CRÍTICA: Não podes usar apenas "pause". Tens de ceder o CPU!
             * Use a função de yield (ex: scheduler_yield(), thread_yield()).
             * Isto permite que as threads do teclado e das aplicações Ring 3 rodem imediatamente.
             */
            scheduler_yield();
            
        }
    }
}