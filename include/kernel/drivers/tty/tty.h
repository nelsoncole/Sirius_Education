/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tty.h
 *    Description: Cabeçalho do Subsistema TTY com suporte a Modo Canónico,
 *                 Controlo de Linha e Sincronização SMP com buffers isolados.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 14/09/2026
 * ============================================================================
 */

#ifndef _TTY_H_
#define _TTY_H_

#include <kernel/lib/stddef.h>
#include <kernel/kernel/core/spinlock.h>

/* Tamanho do Buffer Circular do TTY (Deve ser potência de 2 para otimização) */
#define TTY_BUF_SIZE 256

/* Flags de Configuração de Linha (Estilo termios do POSIX) */
#define TTY_ICANON  0x00000001  /* Ativa Modo Canónico (Orientado a Linhas) */
#define TTY_ECHO    0x00000002  /* Ativa o Eco de Caracteres na Tela */

/**
 * Estrutura de Controlo do Dispositivo TTY
 */
struct tty_device {
    /* Buffer de Entrada (Teclado -> Aplicação) */
    char input_buf[TTY_BUF_SIZE];
    int in_head;
    int in_tail;
    int line_start;
    int raw_count;
    int lines_available;

    /* Buffer de Saída (Aplicação/Echo -> Emulador de Tela) */
    char output_buf[TTY_BUF_SIZE];
    int out_head;
    int out_tail;

    unsigned int c_lflag;
    spinlock_t lock;

    /* ============================================================================
     *                      CAMPOS PARA IMPLEMENTAÇÃO FUTURA (POSIX)
     * ============================================================================ */
    int foreground_pgid; /* ID do Grupo de Processos em primeiro plano (manda na TTY) */
    int session_id;      /* ID da Sessão associada a este terminal (Controlling TTY) */
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

#endif /* _TTY_H_ */
