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

/* Códigos de Controle ASCII */
#define CTRL(c)    ((c) & 0x1F)
#define ASCII_BS   0x08    /* Backspace */
#define ASCII_NL   0x0A    /* Line Feed (\n) */
#define ASCII_CR   0x0D    /* Carriage Return (\r) */
#define ASCII_DEL  0x7F    /* Delete */

#define MAX_TTY_DRV_DEVICES 6

/* Catálogo interno do driver para rastrear os motores de buffer alocados pelo VFS */
static struct tty_device* g_tty_instances[MAX_TTY_DRV_DEVICES];
static uint32_t           g_current_active_id = 0;
static spinlock_t         g_tty_driver_lock;

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
    if (!tty || !user_buf) return -1;

    spin_lock(&tty->lock);
    /* Enfileira sequencialmente a string formatada enviada pelos processos via printf */
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
