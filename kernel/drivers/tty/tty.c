/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tty.c
 *    Description: Subsistema TTY Core com buffers circulares, disciplina de
 *                 linha em Modo Canónico, tratamento de eco e vetores dinâmicos
 *                 para suporte estável a múltiplas CPUs (SMP).
 * 
 *        Author:  Nelson Cole
 *   Created Date: 14/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 23/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/tty/tty.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/klib.h>
#include <kernel/boot_info.h>
#include <kernel/drivers/char/font.h>

/* Códigos de Controle ASCII */
#define CTRL(c)    ((c) & 0x1F)
#define ASCII_BS   0x08    /* Backspace */
#define ASCII_NL   0x0A    /* Line Feed (\n) */
#define ASCII_CR   0x0D    /* Carriage Return (\r) */
#define ASCII_DEL  0x7F    /* Delete */

/* Catálogo interno do driver para rastrear os motores de buffer alocados pelo VFS */
static struct tty_device* g_tty_instances[MAX_TTY_DRV_DEVICES];
static uint32_t           g_current_active_id = 0;
static spinlock_t         g_tty_driver_lock;

extern void *optimized_memset_dword(void *dst, uint32_t value, size_t count);

/* ============================================================================
 *               Função Auxiliar Interna da Fila de Saída
 * ============================================================================ */

/**
 * tty_put_queue - Insere um caractere diretamente na fila de saída do TTY.
 *                 ATENÇÃO: Deve ser chamada sempre com o spinlock da instância adquirido.
 */
static void tty_put_queue(struct tty_device *tty, char c) {
    int next = (tty->out_head + 1) % TTY_BUF_SIZE;
    if (next != tty->out_tail) {
        tty->output_buf[tty->out_head] = c;
        tty->out_head = next;
    }
}

/* ============================================================================
 *                      Implementação das APIs do TTY Core
 * ============================================================================ */

/**
 * tty_init - Inicializa as tabelas do subsistema e locks nativos do driver de hardware.
 *            Chamado uma única vez durante o arranque frio do Kernel.
 */
void tty_init(void) {
    spin_lock_init(&g_tty_driver_lock);
    
    spin_lock(&g_tty_driver_lock);
    for (int i = 0; i < MAX_TTY_DRV_DEVICES; i++) {
        g_tty_instances[i] = NULL;
    }
    g_current_active_id = 0;
    spin_unlock(&g_tty_driver_lock);
}

/**
 * tty_register_driver_instance - Vincula um motor de buffer alocado no VFS ao driver físico.
 * @id:  Índice numérico do terminal (0 a 5).
 * @tty: Endereço físico da estrutura tty_device instanciada por kmalloc.
 */
void tty_register_driver_instance(uint32_t id, struct tty_device* tty) {
    if (id >= MAX_TTY_DRV_DEVICES) return;

    spin_lock(&g_tty_driver_lock);
    g_tty_instances[id] = tty;
    spin_unlock(&g_tty_driver_lock);
}

/**
 * tty_set_active_id - Altera o foco de hardware do terminal ativo de forma atómica.
 */
void tty_set_active_id(uint32_t id) {
    if (id >= MAX_TTY_DRV_DEVICES) return;

    spin_lock(&g_tty_driver_lock);
    g_current_active_id = id;
    spin_unlock(&g_tty_driver_lock);
}

/**
 * tty_get_current - Devolve a TTY focada no ecrã para operações síncronas abertas.
 *                   Bate com o comportamento esperado por tfs_tty_open no teu VFS.
 */
struct tty_device* tty_get_current(void) {
    spin_lock(&g_tty_driver_lock);
    uint32_t id = g_current_active_id;
    struct tty_device* tty = (id < MAX_TTY_DRV_DEVICES) ? g_tty_instances[id] : NULL;
    spin_unlock(&g_tty_driver_lock);
    return tty;
}

/**
 * tty_push_char_isr - Processa e empurra um byte capturado pelo teclado físico (IRQ)
 *                     para dentro da disciplina de linha do terminal alvo.
 */
void tty_push_char_isr(struct tty_device *tty, char c) {
    if (!tty) return;

    spin_lock(&tty->lock);

    /* Normaliza a quebra de linha padrão Unix (Carriage Return -> Line Feed) */
    if (c == ASCII_CR) {
        c = ASCII_NL;
    }

    /* Disciplina de Linha: Processamento em Modo Canónico */
    if (tty->c_lflag & TTY_ICANON) {
        
        /* Tratamento do Backspace Destrutivo */
        if (c == ASCII_BS || c == ASCII_DEL) {
            /* Impede a regressão para além do início do comando atual em buffer */
            if (tty->in_head != tty->line_start) {
                tty->in_head = (tty->in_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
                tty->raw_count--;

                /* ECHO: Repassa comandos ANSI puramente para a cauda de renderização do ecrã */
                if (tty->c_lflag & TTY_ECHO) {
                    tty_put_queue(tty, ASCII_BS); /* Recua o cursor */
                    tty_put_queue(tty, ' ');      /* Limpa o caractere antigo */
                    tty_put_queue(tty, ASCII_BS); /* Reajusta o cursor */
                }
            }
            spin_unlock(&tty->lock);
            return;
        }

        /* Tratamento de Interrupção Assíncrona de Processos em Primeiro Plano (Ctrl+C) */
        if (c == CTRL('c')) {
            if (tty->c_lflag & TTY_ECHO) {
                tty_put_queue(tty, '^');
                tty_put_queue(tty, 'C');
                tty_put_queue(tty, ASCII_NL);
            }
            /* Aborta a linha parcial em RAM limpando o contador cru */
            tty->in_head = tty->line_start;
            tty->raw_count = 0;
            
            spin_unlock(&tty->lock);
            /* TODO: Emitir um sinal SIGINT para o grupo de processos em primeiro plano */
            return;
        }
    }

    /* Insere o caractere regular na cabeça da fila circular de entrada */
    int next = (tty->in_head + 1) % TTY_BUF_SIZE;

    if (next == tty->in_tail) {
        /* Buffer saturado. Aborta a inserção para blindar o Kernel heap */
        spin_unlock(&tty->lock);
        return;
    }

    tty->input_buf[tty->in_head] = c;
    tty->in_head = next;
    tty->raw_count++;

    /* ECHO: Ecoa de volta o caractere digitado instantaneamente */
    if (tty->c_lflag & TTY_ECHO) {
        tty_put_queue(tty, c);
    }

    /* Validação orientada a linhas ou interações brutas */
    if (tty->c_lflag & TTY_ICANON) {
        if (c == ASCII_NL) {
            tty->line_start = tty->in_head; /* Trava o ponto inicial estável da próxima instrução */
            tty->lines_available++;         /* Desbloqueia e acorda consumidores do VFS */
        }
    } else {
        /* Modo RAW: Qualquer caractere fica elegível para consumo de imediato */
        tty->lines_available++;
    }

    spin_unlock(&tty->lock);
}

int tty_read(struct tty_device *tty, char *user_buf, unsigned long size) {
    if (!tty || !user_buf || size == 0) return 0;

    unsigned long bytes_read = 0;

    /* Loop de Bloqueio Passivo: Suspende a CPU até que o hardware entregue dados */
    while (1) {
        spin_lock(&tty->lock);
        
        /* Acorda se houver linhas completas (Canónico) ou bytes soltos (Modo Raw) */
        if (tty->lines_available > 0 || (tty->raw_count > 0 && !(tty->c_lflag & TTY_ICANON))) {
            spin_unlock(&tty->lock);
            break;
        }
        
        spin_unlock(&tty->lock);
        
        /* Coloca o processador local em espera atómica segura para poupar energia */
        __asm__ __volatile__("hlt");
    }

    /* Secção Crítica de Consumo: Retira dados do driver e descarrega na RAM do processo */
    spin_lock(&tty->lock);

    while (tty->in_tail != tty->in_head && bytes_read < size) {
        char c = tty->input_buf[tty->in_tail];

        user_buf[bytes_read++] = c;

        tty->in_tail = (tty->in_tail + 1) % TTY_BUF_SIZE;
        tty->raw_count--;

        /* Condição de paragem estrita do padrão POSIX para buffers em modo canónico */
        if ((tty->c_lflag & TTY_ICANON) && c == ASCII_NL) {
            if (tty->lines_available > 0) {
                tty->lines_available--;
            }
            break;
        }
    }

    /* Atualiza os sinalizadores residuais do Modo RAW se os caracteres acabaram */
    if (!(tty->c_lflag & TTY_ICANON) && tty->raw_count == 0) {
        tty->lines_available = 0;
    }

    spin_unlock(&tty->lock);
    return bytes_read;
}

int tty_write(struct tty_device *tty, const char *user_buf, unsigned long size) {
    if (!tty || !user_buf || size == 0) return -1;

    // 1. O spinlock protege EXCLUSIVAMENTE a inserção de caracteres na fila circular
    spin_lock(&tty->lock);
    for (unsigned long i = 0; i < size; i++) {
        tty_put_queue(tty, user_buf[i]);
    }
    spin_unlock(&tty->lock);
    
    return size;
}

int tty_pop_output(struct tty_device *tty, char *out_c) {
    if (!tty || !out_c) return 0;

    spin_lock(&tty->lock);

    /* Se a fila de saída assíncrona estiver vazia, retorna imediatamente falso */
    if (tty->out_tail == tty->out_head) {
        spin_unlock(&tty->lock);
        return 0;
    }

    /* Remove o caractere mais antigo pendente de renderização física */
    *out_c = tty->output_buf[tty->out_tail];
    tty->out_tail = (tty->out_tail + 1) % TTY_BUF_SIZE;

    spin_unlock(&tty->lock);
    return 1; /* Sucesso */
}

/* ==============================================================================================================
 * Codigo para o emulador do terminal
 * ==============================================================================================================
 */

void tty_init_video_context(struct tty_device* tty, unsigned int pages) {
    
    tty->width = g_boot_info->Graphics.Width;
    tty->height = g_boot_info->Graphics.Height;
    tty->pixels_per_scanLine = g_boot_info->Graphics.PixelsPerScanLine;
    
    // Guarda o número de páginas configurado para esta TTY
    tty->total_pages = (pages > 0) ? pages : 1; 
    
    uint32_t bpp = g_boot_info->Graphics.BitsPerPixel / 8;
    uint32_t pitch = tty->pixels_per_scanLine * bpp;
    
    // Multiplica dinamicamente pelo número de páginas da instância
    tty->buffer_size = (tty->height * tty->total_pages) * pitch;
    tty->video_buffer = (uint32_t*)kmalloc(tty->buffer_size);
    
    tty->cursor_x = 0;
    tty->cursor_y = 0;
    tty->scroll_y_offset = 0; 
    tty->fg_color = 0xFFFFFFFF; 
    tty->bg_color = 0x00000000; 

    uint32_t total_pixels = tty->buffer_size / sizeof(uint32_t);
    /*for (uint32_t i = 0; i < total_pixels; i++) {
        tty->video_buffer[i] = tty->bg_color;
    }*/

    optimized_memset_dword(tty->video_buffer, tty->bg_color, total_pixels);
}

/**
 * put_pixel_to_buffer - Pinta um pixel no buffer isolado da TTY com proteção contra estouros.
 */
static inline void put_pixel_to_buffer(struct tty_device *tty, unsigned int x, unsigned int y, uint32_t color) {
    unsigned int max_virtual_height = tty->height * tty->total_pages;
    
    // Proteção de segurança (Clipping): Impede qualquer pixel de ser gravado fora da RAM alocada
    if (x >= tty->width || y >= max_virtual_height) {
        return;
    }

    unsigned int offset = (y * tty->pixels_per_scanLine) + x;
    tty->video_buffer[offset] = color;
}

/**
 * tty_clear_buffer - Limpa o buffer dedicado completo (todas as páginas) de uma TTY,
 *                    resetando os cursores e a janela de visualização para o topo.
 */
void tty_clear_buffer(struct tty_device *tty) {
    if (!tty || !tty->video_buffer) return;

    // 1. Calcula o total de pixels contidos em TODAS as páginas lógicas alocadas
    unsigned int max_virtual_height = tty->height * tty->total_pages;
    uint32_t total_pixels = max_virtual_height * tty->pixels_per_scanLine;

    // 2. Preenche todo o Backbuffer virtual com a cor de fundo usando SIMD ultra-rápido
    optimized_memset_dword(tty->video_buffer, tty->bg_color, total_pixels);

    // 3. Reseta de forma síncrona os cursores locais e o offset de visão para o ponto zero
    tty->cursor_x = 0; 
    tty->cursor_y = 0; 
    tty->scroll_y_offset = 0;

    // 4. Sinaliza ao emulador gráfico para sincronizar esta limpeza com a VRAM física
    __sync_lock_test_and_set(&tty->refresh_needed, 1);
}

/**
 * tty_clear_line - Limpa uma linha específica de pixels truncando a escrita antes do fim do buffer.
 */
static void tty_clear_line(struct tty_device *tty, unsigned int start_y) {
    unsigned int max_virtual_height = tty->height * tty->total_pages;
    
    if (start_y >= max_virtual_height) return;

    // Se o bloco de 16 linhas da fonte for ultrapassar a altura máxima do buffer virtual,
    // trunca a quantidade de linhas para apagar apenas a porção restante legítima.
    unsigned int linhas_para_limpar = FONT_HEIGHT;
    if (start_y + linhas_para_limpar > max_virtual_height) {
        linhas_para_limpar = max_virtual_height - start_y;
    }

    uint32_t *line_ptr = tty->video_buffer + (start_y * tty->pixels_per_scanLine);
    uint32_t pixels_to_clear = linhas_para_limpar * tty->pixels_per_scanLine;
    
    // Executa a limpeza SIMD ultra-rápida de forma 100% segura dentro dos limites
    optimized_memset_dword(line_ptr, tty->bg_color, pixels_to_clear);
}

void tty_putc_backbuffer(struct tty_device *tty, char c) {
    unsigned int max_virtual_height = tty->height * tty->total_pages;
    int nova_linha = 0;

    /* ====================================================================
     * 1. GESTÃO DE ESTOURO ANTECIPADA (WIPE SE ESTIVER SEM ESPAÇO)
     * ==================================================================== */
    // Se o cursor já estiver posicionado num ponto onde a próxima escrita 
    // ultrapassa o limite absoluto das páginas, limpa tudo ANTES de processar.
    if ((unsigned int)tty->cursor_y + FONT_HEIGHT > max_virtual_height) {
        tty_clear_buffer(tty); 
        // Nota: tty_clear_buffer já reseta cursor_x, cursor_y e scroll_y_offset para 0
    }

    /* ====================================================================
     * 2. PROCESSAMENTO DE CARACTERES DE CONTROLO E AVANÇO DE CURSOR
     * ==================================================================== */
    if (c == '\n') {
        tty->cursor_x = 0;
        tty->cursor_y += FONT_HEIGHT;
        nova_linha = 1;
    }
    else if (c == '\r') {
        tty->cursor_x = 0;
        return;
    }
    else if (c == '\t') {
        tty->cursor_x += (FONT_WIDTH * 4);
    }
    else {
        uint8_t font_index = (uint8_t)c;
        if (font_index >= 128) return;

        // Desenha o caractere de forma segura (put_pixel_to_buffer faz o clipping)
        for (int cy = 0; cy < FONT_HEIGHT; cy++) {
            uint8_t line = g_font_bitmap[font_index][cy];
            for (int cx = 0; cx < FONT_WIDTH; cx++) {
                uint32_t color = (line & (0x80 >> cx)) ? tty->fg_color : tty->bg_color;
                put_pixel_to_buffer(tty, (unsigned int)tty->cursor_x + cx, (unsigned int)tty->cursor_y + cy, color);
            }
        }
        tty->cursor_x += FONT_WIDTH;
    }

    /* ====================================================================
     * 3. QUEBRA AUTOMÁTICA DE LINHA (WRAP)
     * ==================================================================== */
    if ((unsigned int)tty->cursor_x + FONT_WIDTH > tty->width) {
        tty->cursor_x = 0;
        tty->cursor_y += FONT_HEIGHT;
        nova_linha = 1;
    }

    // Se mudou de linha e ainda estamos dentro dos limites, limpa preventivamente a linha
    // (Nota: Se o cursor passar do limite aqui, o próximo caractere a entrar tratará do Wipe no Passo 1)
    if (nova_linha && ((unsigned int)tty->cursor_y + FONT_HEIGHT <= max_virtual_height)) {
        tty_clear_line(tty, (unsigned int)tty->cursor_y);
    }

    /* ====================================================================
     * 4. AJUSTE DA JANELA DESLIZANTE DE REFRESH (JANELA VISÍVEL)
     * ==================================================================== */
    if ((unsigned int)tty->cursor_y + FONT_HEIGHT > tty->scroll_y_offset + tty->height) {
        tty->scroll_y_offset = ((unsigned int)tty->cursor_y + FONT_HEIGHT) - tty->height;
    } 
    else if ((unsigned int)tty->cursor_y < tty->scroll_y_offset) {
        tty->scroll_y_offset = (unsigned int)tty->cursor_y;
    }

    /* ====================================================================
     * CORREÇÃO DE DE SINCRONIZAÇÃO: SINALIZAÇÃO ATÓMICA PÓS-DESENHO
     * ==================================================================== 
     * Só aqui, com os pixéis do glifo gravados e as quebras e offsets calculados,
     * é que marcamos de forma atómica a flag de refresh. O emulador nunca mais 
     * vai fazer um flush parcial ou obsoleto!
     */
    __sync_lock_test_and_set(&tty->refresh_needed, 1);
}

void tty_putc_backbuffer_X(struct tty_device *tty, char c) {
    unsigned int max_virtual_height = tty->height * tty->total_pages;
    int nova_linha = 0;

    /* ====================================================================
     * 1. GESTÃO DE ESTOURO ANTECIPADA (WIPE SE ESTIVER SEM ESPAÇO)
     * ==================================================================== */
    // Se o cursor já estiver posicionado num ponto onde a próxima escrita 
    // ultrapassa o limite absoluto das páginas, limpa tudo ANTES de processar.
    if ((unsigned int)tty->cursor_y + FONT_HEIGHT > max_virtual_height) {
        tty_clear_buffer(tty); 
        // Nota: tty_clear_buffer já reseta cursor_x, cursor_y e scroll_y_offset para 0
    }

    /* ====================================================================
     * 2. PROCESSAMENTO DE CARACTERES DE CONTROLO E AVANÇO DE CURSOR
     * ==================================================================== */
    if (c == '\n') {
        tty->cursor_x = 0;
        tty->cursor_y += FONT_HEIGHT;
        nova_linha = 1;
    }
    else if (c == '\r') {
        tty->cursor_x = 0;
        return;
    }
    else if (c == '\t') {
        tty->cursor_x += (FONT_WIDTH * 4);
    }
    else {
        uint8_t font_index = (uint8_t)c;
        if (font_index >= 128) return;

        // Desenha o caractere de forma segura (put_pixel_to_buffer faz o clipping)
        for (int cy = 0; cy < FONT_HEIGHT; cy++) {
            uint8_t line = g_font_bitmap[font_index][cy];
            for (int cx = 0; cx < FONT_WIDTH; cx++) {
                uint32_t color = (line & (0x80 >> cx)) ? tty->fg_color : tty->bg_color;
                put_pixel_to_buffer(tty, (unsigned int)tty->cursor_x + cx, (unsigned int)tty->cursor_y + cy, color);
            }
        }
        tty->cursor_x += FONT_WIDTH;
    }

    /* ====================================================================
     * 3. QUEBRA AUTOMÁTICA DE LINHA (WRAP)
     * ==================================================================== */
    if ((unsigned int)tty->cursor_x + FONT_WIDTH > tty->width) {
        tty->cursor_x = 0;
        tty->cursor_y += FONT_HEIGHT;
        nova_linha = 1;
    }

    // Se mudou de linha e ainda estamos dentro dos limites, limpa preventivamente a linha
    // (Nota: Se o cursor passar do limite aqui, o próximo caractere a entrar tratará do Wipe no Passo 1)
    if (nova_linha && ((unsigned int)tty->cursor_y + FONT_HEIGHT <= max_virtual_height)) {
        tty_clear_line(tty, (unsigned int)tty->cursor_y);
    }

    /* ====================================================================
     * 4. AJUSTE DA JANELA DESLIZANTE DE REFRESH (JANELA VISÍVEL)
     * ==================================================================== */
    if ((unsigned int)tty->cursor_y + FONT_HEIGHT > tty->scroll_y_offset + tty->height) {
        tty->scroll_y_offset = ((unsigned int)tty->cursor_y + FONT_HEIGHT) - tty->height;
    } 
    else if ((unsigned int)tty->cursor_y < tty->scroll_y_offset) {
        tty->scroll_y_offset = (unsigned int)tty->cursor_y;
    }
}

