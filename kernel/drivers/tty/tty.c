/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tty.c
 *    Description: Subsistema TTY com buffers isolados de entrada/saída,
 *                 suporte a Modo Canónico e sincronização atómica para SMP.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 14/09/2026
 * ============================================================================
 */

#include <kernel/drivers/tty/tty.h>
#include <kernel/kernel/core/spinlock.h>

/* Códigos de Controle ASCII */
#define CTRL(c)    ((c) & 0x1F)
#define ASCII_BS   0x08    /* Backspace */
#define ASCII_NL   0x0A    /* Line Feed (\n) */
#define ASCII_CR   0x0D    /* Carriage Return (\r) */
#define ASCII_DEL  0x7F    /* Delete */

static struct tty_device g_main_tty;

/* ============================================================================
 *               Função Auxiliar Interna da Fila de Saída
 * ============================================================================ */

/**
 * tty_put_queue - Insere um caractere diretamente na fila de saída do TTY.
 *                 ATENÇÃO: Deve ser chamada sempre com o spinlock já adquirido.
 */
static void tty_put_queue(struct tty_device *tty, char c) {
    int next = (tty->out_head + 1) % TTY_BUF_SIZE;
    if (next != tty->out_tail) {
        tty->output_buf[tty->out_head] = c;
        tty->out_head = next;
    }
}

/* ============================================================================
 *                      Implementação das APIs do TTY
 * ============================================================================ */

void tty_init(void) {
    struct tty_device *tty = &g_main_tty;

    tty->in_head = 0;
    tty->in_tail = 0;
    tty->line_start = 0;
    tty->raw_count = 0;
    tty->lines_available = 0;

    tty->out_head = 0;
    tty->out_tail = 0;

    /* Ativa por padrão o comportamento clássico de terminal (ICANON + ECHO) */
    tty->c_lflag = TTY_ICANON | TTY_ECHO;

    /* Inicializa o trinco SMP */
    spin_lock_init(&tty->lock);
}

struct tty_device* tty_get_current(void) {
    return &g_main_tty;
}

void tty_push_char_isr(struct tty_device *tty, char c) {
    if (!tty) return;

    /* 
     * Contexto de interrupção (IRQ): Não desativamos IRQs locais de forma forçada,
     * apenas adquirimos o lock para evitar conflito com syscalls em execução noutros cores.
     */
    spin_lock(&tty->lock);

    /* Normaliza a quebra de linha (Carriage Return -> Line Feed) */
    if (c == ASCII_CR) {
        c = ASCII_NL;
    }

    /* Disciplina de Linha: Processamento em Modo Canónico */
    if (tty->c_lflag & TTY_ICANON) {
        
        /* Tratamento do Backspace Destrutivo */
        if (c == ASCII_BS || c == ASCII_DEL) {
            /* Só permite recuar o buffer até ao início do comando atual */
            if (tty->in_head != tty->line_start) {
                tty->in_head = (tty->in_head - 1 + TTY_BUF_SIZE) % TTY_BUF_SIZE;
                tty->raw_count--;

                /* ECHO: Injeta a sequência física de deleção puramente na fila de saída */
                if (tty->c_lflag & TTY_ECHO) {
                    tty_put_queue(tty, ASCII_BS); /* Move o cursor da tela para trás */
                    tty_put_queue(tty, ' ');      /* Substitui o caractere antigo por espaço */
                    tty_put_queue(tty, ASCII_BS); /* Reajusta o cursor da tela novamente */
                }
            }
            spin_unlock(&tty->lock);
            return;
        }

        /* Tratamento de Interrupção de Processo (Ctrl+C) */
        if (c == CTRL('c')) {
            if (tty->c_lflag & TTY_ECHO) {
                tty_put_queue(tty, '^');
                tty_put_queue(tty, 'C');
                tty_put_queue(tty, ASCII_NL);
            }
            /* Descarta a linha incompleta e reinicia o ponteiro de input */
            tty->in_head = tty->line_start;
            tty->raw_count = 0;
            spin_unlock(&tty->lock);
            return;
        }
    }

    /* Insere o caractere comum no input_buf */
    int next = (tty->in_head + 1) % TTY_BUF_SIZE;

    if (next == tty->in_tail) {
        /* Buffer de entrada cheio. Ignora o byte para evitar estouro de memória */
        spin_unlock(&tty->lock);
        return;
    }

    tty->input_buf[tty->in_head] = c;
    tty->in_head = next;
    tty->raw_count++;

    /* ECHO: Transfere o caractere digitado imediatamente para a fila de saída */
    if (tty->c_lflag & TTY_ECHO) {
        tty_put_queue(tty, c);
    }

    /* Fecho e validação da string orientada a linhas */
    if (tty->c_lflag & TTY_ICANON) {
        if (c == ASCII_NL) {
            tty->line_start = tty->in_head; /* Fixa o limite seguro do próximo comando */
            tty->lines_available++;         /* Sinaliza uma linha inteira pronta para leitura */
            
            /* TODO: Se tiver escalonador, coloque aqui a notificação de desbloqueio de processo */
        }
    } else {
        /* Modo RAW: Qualquer caractere fica disponível para consumo imediatamente */
        tty->lines_available++;
    }

    spin_unlock(&tty->lock);
}

int tty_read(struct tty_device *tty, char *user_buf, unsigned long size) {
    if (!tty || !user_buf || size == 0) return 0;

    unsigned long bytes_read = 0;

    /* Loop de bloqueio controlado: Aguarda pacientemente pela entrada de dados */
    while (1) {
        spin_lock(&tty->lock);
        
        /* Condições para acordar: Linha disponível (Canónico) ou caractere solto (RAW) */
        if (tty->lines_available > 0 || (tty->raw_count > 0 && !(tty->c_lflag & TTY_ICANON))) {
            spin_unlock(&tty->lock);
            break;
        }
        
        spin_unlock(&tty->lock);
        
        /* Coloca o core local em repouso passivo até ao próximo sinal do teclado */
        __asm__ __volatile__("hlt");
    }

    /* Secção crítica de extração: Copia os dados seguros para o Ring 3 */
    spin_lock(&tty->lock);

    while (tty->in_tail != tty->in_head && bytes_read < size) {
        char c = tty->input_buf[tty->in_tail];

        user_buf[bytes_read++] = c;

        tty->in_tail = (tty->in_tail + 1) % TTY_BUF_SIZE;
        tty->raw_count--;

        /* Interrompe estritamente no caractere de nova linha em modo canónico */
        if ((tty->c_lflag & TTY_ICANON) && c == ASCII_NL) {
            if (tty->lines_available > 0) {
                tty->lines_available--;
            }
            break;
        }
    }

    /* Fallback automático para contabilidade de leitura em Modo RAW */
    if (!(tty->c_lflag & TTY_ICANON) && tty->raw_count == 0) {
        tty->lines_available = 0;
    }

    spin_unlock(&tty->lock);
    return bytes_read;
}

int tty_write(struct tty_device *tty, const char *user_buf, unsigned long size) {
    if (!tty || !user_buf) return -1;

    spin_lock(&tty->lock);
    /* Copia os dados do utilizador sequencialmente para a fila de saída assíncrona */
    for (unsigned long i = 0; i < size; i++) {
        tty_put_queue(tty, user_buf[i]);
    }
    spin_unlock(&tty->lock);
    
    return size;
}

int tty_pop_output(struct tty_device *tty, char *out_c) {
    if (!tty || !out_c) return 0;

    spin_lock(&tty->lock);

    /* Se a fila estiver vazia, retorna falso imediatamente */
    if (tty->out_tail == tty->out_head) {
        spin_unlock(&tty->lock);
        return 0;
    }

    /* Remove o byte mais antigo e avança o ponteiro de leitura da cauda */
    *out_c = tty->output_buf[tty->out_tail];
    tty->out_tail = (tty->out_tail + 1) % TTY_BUF_SIZE;

    spin_unlock(&tty->lock);
    return 1; /* Sucesso */
}