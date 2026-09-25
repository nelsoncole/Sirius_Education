/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tty.h
 *    Description: Cabeçalho do Subsistema TTY com suporte a Modo Canónico,
 *                 Controlo de Linha e Sincronização SMP com buffers isolados.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 22/09/2026
 * ============================================================================
 */

#ifndef _TTY_H_
#define _TTY_H_

#include <kernel/lib/stddef.h>
#include <kernel/lib/stdint.h>
#include <kernel/kernel/core/spinlock.h>

/* Tamanho do Buffer Circular do TTY (Deve ser potência de 2 para otimização) */
#define TTY_BUF_SIZE 256

/* Flags de Configuração de Linha (Estilo termios do POSIX) */
#define TTY_ICANON  0x00000001  /* Ativa Modo Canónico (Orientado a Linhas) */
#define TTY_ECHO    0x00000002  /* Ativa o Eco de Caracteres na Tela */

#define MAX_TTY_DRV_DEVICES 4

/**
 * Estrutura de Controlo do Dispositivo TTY
 */
struct tty_device {
    /* Buffer de Entrada (Teclado -> Aplicação) */
    char input_buf[TTY_BUF_SIZE];
    volatile int in_head;
    volatile int in_tail;
    volatile int line_start;
    volatile int raw_count;
    volatile int lines_available;

    /* Buffer de Saída (Aplicação/Echo -> Emulador de Tela) */
    char output_buf[TTY_BUF_SIZE];
    volatile int out_head;
    volatile int out_tail;

    uint32_t c_lflag;
    spinlock_t lock;

    /* Campos para video de dedicado */
    uint32_t* video_buffer;     /* Ponteiro para o Backbuffer privado (Alocado via kmalloc) */
    uint32_t  buffer_size;      /* Tamanho total em bytes deste buffer (Width * Height * 4) */

    /* Geometria Virtual da TTY (Agnóstica ao modo gráfico global) */
    uint32_t width;            /* Largura virtual em pixéis (ex: 1024) */
    uint32_t height;           /* Altura virtual em pixéis (ex: 768) */
    uint32_t pixels_per_scanLine; /* Pixéis por linha no buffer virtual */

    // Cada TTY define quantas páginas de memória quer usar
    uint32_t total_pages;      /* 1 para sem scroll, 2 ou 3 para com scroll */
    
    /* Posição atual do cursor */
    uint32_t cursor_x;                  
    uint32_t cursor_y;    /* Vai rodar circularmente entre 0 e (height * TTY_PAGES) */
    
    /* Configuração de Cores da TTY em Formato Raw Pixel (Ex: 32-bit RGBA) */
    uint32_t fg_color;          /* Cor do texto (Foreground) */
    uint32_t bg_color;          /* Cor do fundo (Background) */

     /* Onde começa a linha do ecrã atual a ser renderizada no refresh */
    unsigned int scroll_y_offset;  /* Linha Y de início da janela visível (0 a height * TTY_PAGES) */

    // Sinaliza que o buffer de vídeo privado foi alterado e precisa de ir para o ecrã físico
    volatile int refresh_needed;
};

/* ============================================================================
 *                      APIs de Inicialização e Fluxo
 * ============================================================================ */

/**
 * tty_init - Inicializa a estrutura global da TTY principal, zera os índices,
 *            configura as flags padrões e prepara o spinlock.
 */
void tty_init(void);

/**
 * tty_get_current - Retorna o ponteiro para a instância ativa do TTY.
 *                   Essencial para ser mapeado pelos File Descriptors (VFS) no Ring 3.
 */
struct tty_device* tty_get_current(void);

/**
 * tty_register_driver_instance - Vincula um motor de buffer alocado no VFS ao driver físico.
 * @id:  Índice numérico do terminal (0 a 5).
 * @tty: Endereço físico da estrutura tty_device instanciada por kmalloc.
 */
void tty_register_driver_instance(uint32_t id, struct tty_device* tty);

/**
 * tty_set_active_id - Altera o foco de hardware do terminal ativo de forma atómica.
 */
void tty_set_active_id(uint32_t id);

/**
 * tty_get_current - Devolve a TTY focada no ecrã para operações síncronas abertas.
 *                   Bate com o comportamento esperado por tfs_tty_open no teu VFS.
 */
struct tty_device* tty_get_current(void);

/* ============================================================================
 *                      APIs de Entrada / Saída de Dados
 * ============================================================================ */

/**
 * tty_push_char_isr - Injeta um caractere vindo do driver do teclado.
 *                     Esta função é executada no contexto de interrupção (Ring 0 / IRQ 1)
 *                     e processa os dados sem tocar diretamente no ecrã.
 */
void tty_push_char_isr(struct tty_device *tty, char c);

/**
 * tty_read - Consome dados do buffer do TTY e transfere para o espaço do utilizador.
 *            Mapeado diretamente na Syscall SYS_READ (fd = 0 / stdin).
 */
int tty_read(struct tty_device *tty, char *user_buf, unsigned long size);

/**
 * tty_write - Envia uma string de bytes direto para o buffer de saída da TTY.
 *             Mapeado diretamente na Syscall SYS_WRITE (fd = 1 ou 2 / stdout/stderr).
 */
int tty_write(struct tty_device *tty, const char *user_buf, unsigned long size);

/* ============================================================================
 *                      Interface com o Emulador de Tela
 * ============================================================================ */

/**
 * tty_pop_output - Consome um único byte do buffer de saída do TTY.
 *                  Usado pelo emulador de tela gráfica (fora da ISR) para atualizar os pixels.
 *                  Retorna 1 se um caractere foi retirado com sucesso, ou 0 se a fila estiver vazia.
 */
int tty_pop_output(struct tty_device *tty, char *out_c);


void tty_init_video_context(struct tty_device* tty, unsigned int pages);
void tty_putc_backbuffer(struct tty_device *tty, char c);

#endif /* _TTY_H_ */
