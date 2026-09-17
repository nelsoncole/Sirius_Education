/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: console.c
 *    Description: Thread de Kernel do Emulador de Tela Assíncrono.
 *                 Consome a fila de saída da TTY e atualiza o hardware via kprintf.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 15/09/2026
 * ============================================================================
 */

#include <kernel/fs/dev/vfs_tty.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/drivers/char/keyboard.h>
#include <kernel/klib.h>

/* Códigos de Controle ASCII herdados da TTY */
#define ASCII_BS   0x08
#define ASCII_NL   0x0A
#define ASCII_CR   0x0D

/**
 * tty_emulator_thread - Ponto de entrada da Thread de Kernel do Emulador.
 *                       Roda em segundo plano no Ring 0 com IRQs ativas.
 */

extern void kprintf2(const char *format, ...);
void tty_emulator_thread() {

    /* 1. Obtém o nó e a estrutura de controlo da TTY ativa */
    vfs_node_t* tty_node = tty_vfs_get_node();
    while (!tty_node) {
        // Salvaguarda caso a thread inicie antes da TTY ser registada no VFS
        tty_node = tty_vfs_get_node();
    }

    struct tty_device* tty = (struct tty_device*)tty_node->private_data;
    char c;

    /* 2. LAÇO INFINITO DE EXECUÇÃO DA KTHREAD */
    while (1) {
        /* 
         * 3. Sincronização por Consumo: Enquanto houver caracteres no output_buf,
         *    retira-os de forma segura (respeitando o Spinlock SMP interno).
         */
        int teve_dados = 0;
        
        while (tty_pop_output(tty, &c)) {
            teve_dados = 1;

            /* 4. O Emulador interpreta os bytes e passa para o renderizador kprintf */
            if (c == ASCII_BS) {
                kprintf2("\b");
            } 
            else if (c == ASCII_NL) {
                kprintf2("\n");
            } 
            else if (c == ASCII_CR) {
                kprintf2("\r");
            } 
            else {
                kprintf2("%c", c);
            }
        }

        /* 
         * 5. POLÍTICA DE COOPERAÇÃO/DESCANSO:
         * Se o buffer esvaziou, a thread deve render o CPU para não fritar o núcleo a 100%.
         * Dependendo do seu Kernel:
         * - Se tiver uma chamada de sleep/delay: thread_sleep(10); // Dorme 10ms
         * - Se tiver yield: scheduler_yield(); // Cede tempo para outros processos
         * - Fallback provisório: asm("hlt") se as interrupções estiverem ligadas.
         */
        if (!teve_dados) {
            // Se o seu agendador suportar yield, use-o aqui para dar tempo à Shell
            // scheduler_yield(); 
            
            // Fallback físico seguro enquanto não usa yield/sleep de thread:
            __asm__ __volatile__("pause");
        }
    }
}

/* Tabela base: Minúsculas e números normais */
static const char g_kbd_us_keymap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,   '*',   0,
   ' '
};

/* Tabela Shift: Maiúsculas e símbolos superiores */
static const char g_kbd_us_shift_keymap[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,   '*',   0,
   ' '
};

/**
 * tty_keyboard_bridge_thread - Thread de Kernel responsável por bombear as teclas para o TTY.
 *                              Consome a estrutura de eventos atómica de 16-bits.
 */
void tty_keyboard_bridge_thread() {

    /* 1. Aguarda e obtém o nó global da TTY unificada */
    vfs_node_t* tty_node = tty_vfs_get_node();
    while (!tty_node) {
        tty_node = tty_vfs_get_node();
    }
    struct tty_device* tty = (struct tty_device*)tty_node->private_data;

    while (1) {
        /* 2. BLOQUEIO SEGURO: Aguarda o snapshot estruturado do evento vindo da ISR */
        kbd_event_t event = keyboard_get_event();
        
        /* 3. Extrai e valida o scancode base puro (Set 1) */
        uint8_t scancode = event.scancode;
        if (scancode >= 128) continue;

        /* 4. FILTRO DE EVENTOS: Se a tecla foi libertada (Key Up / Release), ignora */
        if (event.modifiers_state & KBD_MOD_RELEASE) {
            continue; 
        }

        /* 5. Avalia o estado dos modificadores gravados no snapshot do evento */
        int shift_active = (event.modifiers_state & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) ? 1 : 0;
        int caps_active  = (event.modifiers_state & KBD_MOD_CAPS) ? 1 : 0;

        char ascii_char = 0;

        /* 6. Seleciona a tabela de caracteres base baseado no estado do Shift */
        if (shift_active) {
            ascii_char = g_kbd_us_shift_keymap[scancode];
        } else {
            ascii_char = g_kbd_us_keymap[scancode];
        }

        /* 7. Regra do Caps Lock: Só inverte letras de 'a'-'z' ou 'A'-'Z' */
        if (caps_active && ascii_char != 0) {
            if (ascii_char >= 'a' && ascii_char <= 'z') {
                /* Se o Shift também estiver ativo, o Caps anula-o voltando a ser minúscula */
                ascii_char = shift_active ? ascii_char : (ascii_char - 32);
            } 
            else if (ascii_char >= 'A' && ascii_char <= 'Z') {
                ascii_char = shift_active ? ascii_char : (ascii_char + 32);
            }
        }

        /* 8. INJEÇÃO ATÓMICA NA TTY: Se for um caractere ASCII legítimo, envia à TTY */
        if (ascii_char != 0) {
            tty_push_char_isr(tty, ascii_char);
        }
    }
}