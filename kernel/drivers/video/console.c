/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: console.c
 *    Description: Thread de Kernel do Emulador de Tela Assíncrono e Bridge do
 *                 Teclado com suporte nativo a chaveamento dinâmico de TTYs (F1-F6).
 *                 Consome as filas de saída das TTYs dinamicamente a partir do foco.
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
#include <kernel/drivers/char/keyboard.h>
#include <kernel/klib.h>

/* Códigos de Controle ASCII herdados da TTY */
#define ASCII_BS   0x08
#define ASCII_NL   0x0A
#define ASCII_CR   0x0D

/* Definições de Scancodes padrão IBM Set 1 para Teclas de Função F1-F6 */
#define SCANCODE_F1   0x3B
#define SCANCODE_F2   0x3C
#define SCANCODE_F3   0x3D
#define SCANCODE_F4   0x3E
#define SCANCODE_F5   0x3F
#define SCANCODE_F6   0x40

extern void kprintf2(const char *format, ...);

/**
 * tty_emulator_thread - Ponto de entrada da Thread de Kernel do Emulador.
 *                       Roda em segundo plano consumindo a TTY ativa no ecrã.
 */
void tty_emulator_thread() {
    char c;

    /* LAÇO INFINITO DE EXECUÇÃO DA KTHREAD */
    while (1) {
        int teve_dados = 0;
        
        /* 1. POLIMORFISMO: Resolve dinamicamente quem é a TTY ativa no monitor AGORA */
        vfs_node_t* active_node = tty_vfs_get_active_node();
        
        if (active_node && active_node->private_data) {
            struct tty_device* tty = (struct tty_device*)active_node->private_data;

            /* 2. Sincronização por Consumo: Esvazia a fila de output do terminal focado */
            while (tty_pop_output(tty, &c)) {
                teve_dados = 1;

                /* 3. O Emulador interpreta os bytes e passa para o renderizador físico */
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
        }

        /* 4. POLÍTICA DE COOPERAÇÃO: Evita que a KThread frite o núcleo de CPU */
        if (!teve_dados) {
            // Se o Sirius_Education tiver yield, descomente aqui:
            // scheduler_yield(); 
            
            // Fallback passivo de baixo consumo
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
 *                              Captura e intercepta F1-F6 para chaveamento dinâmico.
 */
void tty_keyboard_bridge_thread() {

    while (1) {
        /* 1. BLOQUEIO SEGURO: Aguarda o snapshot estruturado do evento vindo da ISR */
        kbd_event_t event = keyboard_get_event();
        
        /* 2. Extrai e valida o scancode base puro (Set 1) */
        uint8_t scancode = event.scancode;
        if (scancode >= 128) continue;

        /* 3. FILTRO DE EVENTOS: Se a tecla foi libertada (Key Up / Release), ignora */
        if (event.modifiers_state & KBD_MOD_RELEASE) {
            continue; 
        }

        /* 4. REGRA DE INTERCEPÇÃO: Verifica se o utilizador premiu F1 a F6 para mudar de terminal */
        if (scancode >= SCANCODE_F1 && scancode <= SCANCODE_F6) {
            uint32_t target_tty_id = scancode - SCANCODE_F1; // Converte para índice (0 a 5)
            
            if (target_tty_id < MAX_TTY_DEVICES) {
                kprintf("\n[CONSOLE] A alternar de terminal para /dev/tty%d...\n", target_tty_id);
                
                /* Executa a viragem de foco atómica de forma síncrona nos dois sub-sistemas */
                tty_vfs_set_active_index(target_tty_id); // Atualiza o VFS do Core
                tty_set_active_id(target_tty_id);        // Atualiza a tabela do driver físico
                
                /* TODO: Se possuir suporte gráfico ou buffers de ecrã virtuais isolados por TTY, 
                         mande redesenhar/limpar o ecrã físico aqui. */
            }
            continue; // Consome o scancode sem enviar o caractere de controle para a Shell
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
                ascii_char = shift_active ? ascii_char : (ascii_char - 32);
            } 
            else if (ascii_char >= 'A' && ascii_char <= 'Z') {
                ascii_char = shift_active ? ascii_char : (ascii_char + 32);
            }
        }

        /* 8. INJEÇÃO DINÂMICA: Obtém o TTY focado no momento e joga os bytes nele */
        if (ascii_char != 0) {
            struct tty_device* tty_focado = tty_get_current();
            if (tty_focado) {
                tty_push_char_isr(tty_focado, ascii_char);
            }
        }
    }
}