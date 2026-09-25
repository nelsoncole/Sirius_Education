/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_tty_kbd.c
 *    Description: Bridge do Teclado com suporte nativo a chaveamento dinâmico
 *                 de TTYs (F1-F6). Desenho assíncrono delegado ao emulador.
 *
 *        Author:  Nelson Cole
 *   Created Date: 23/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 23/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/dev/vfs_tty.h>
#include <kernel/fs/dev/vfs_pty.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/drivers/char/keyboard.h>
#include <kernel/klib.h>

/* Definições de Scancodes padrão IBM Set 1 para Teclas de Função F1-F4 */
#define SCANCODE_F1 0x3B
#define SCANCODE_F4 0x3E

extern void tty_putc_backbuffer(struct tty_device *tty, char c);

/* Tabela base: Minúsculas e números normais */
static const char g_kbd_us_keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' '};

/* Tabela Shift: Maiúsculas e símbolos superiores */
static const char g_kbd_us_shift_keymap[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0,
    ' '};

/**
 * tty_keyboard_bridge_thread - Thread de Kernel responsável por bombear as teclas para o TTY.
 *                              Captura e intercepta F1-F6 para chaveamento dinâmico.
 */
void tty_keyboard_bridge_thread()
{

    while (1)
    {
        /* 1. BLOQUEIO SEGURO: Aguarda o snapshot estruturado do evento vindo da ISR */
        kbd_event_t event = keyboard_get_event();

        /* 2. Extrai e valida o scancode base puro (Set 1) */
        uint8_t scancode = event.scancode;
        if (scancode >= 128)
            continue;

        /* 3. FILTRO DE EVENTOS: Se a tecla foi libertada (Key Up / Release), ignora */
        if (event.modifiers_state & KBD_MOD_RELEASE)
        {
            continue;
        }

        /* 4. REGRA DE INTERCEPÇÃO: Verifica se o utilizador premiu F1 a F6 para mudar de terminal */
        if (scancode >= SCANCODE_F1 && scancode <= SCANCODE_F4)
        {
            uint32_t target_tty_id = scancode - SCANCODE_F1; // Converte para índice (0 a 5)

            if (target_tty_id < MAX_TTY_DRV_DEVICES)
            {
                /* Executa a viragem de foco atómica de forma síncrona nos dois sub-sistemas */
                tty_vfs_set_active_index(target_tty_id); // Atualiza o VFS do Core
                tty_set_active_id(target_tty_id);        // Atualiza a tabela do driver físico

                // O tty_flush_to_screen foi removido daqui com sucesso.
                // O emulador gráfico encarrega-se de redesenhar a nova TTY na próxima iteração.
            }
            continue; // Consome o scancode sem enviar o caractere de controle para a Shell
        }

        /* 5. Avalia o estado dos modificadores gravados no snapshot do evento */
        int shift_active = (event.modifiers_state & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) ? 1 : 0;
        int caps_active = (event.modifiers_state & KBD_MOD_CAPS) ? 1 : 0;

        char ascii_char = 0;

        /* 6. Seleciona a tabela de caracteres base baseado no estado do Shift */
        if (shift_active)
        {
            ascii_char = g_kbd_us_shift_keymap[scancode];
        }
        else
        {
            ascii_char = g_kbd_us_keymap[scancode];
        }

        /* 7. Regra do Caps Lock: Só inverte letras de 'a'-'z' ou 'A'-'Z' */
        if (caps_active && ascii_char != 0)
        {
            if (ascii_char >= 'a' && ascii_char <= 'z')
            {
                ascii_char = shift_active ? ascii_char : (ascii_char - 32);
            }
            else if (ascii_char >= 'A' && ascii_char <= 'Z')
            {
                ascii_char = shift_active ? ascii_char : (ascii_char + 32);
            }
        }

        /* 8. INJEÇÃO DINÂMICA: Desvia o teclado para a PTY ativa, independentemente de qual TTY tem o foco */
        if (ascii_char != 0)
        {
            struct tty_device *tty_focado = tty_get_current();

            if (tty_focado)
            {
                // Obtém diretamente a PTY que detém o foco lógico em background
                struct tty_device *pty_ativa = pty_vfs_get_active_engine();

                /* EXECUÇÃO DO DESVIO DIRETO PARA A PTY ATIVA */
                if (pty_ativa)
                {
                    // 1. Injeta o caractere diretamente na entrada (in_buf) da PTY ativa (O Bash lê na hora!)
                    tty_push_char_isr(pty_ativa, ascii_char);

                    // 2. Se o eco local estiver ativo na PTY, replica o caractere na tty0 fixa de texto
                    // para que o utilizador veja no ecrã F1 o comando a ser digitado
                    if (pty_ativa->c_lflag & TTY_ECHO)
                    {
                        vfs_node_t *tty0_node = tty_vfs_get_node_by_name("tty0");
                        if (tty0_node && tty0_node->private_data)
                        {
                            struct tty_device *tty0_dev = (struct tty_device *)tty0_node->private_data;

                            tty_push_char_isr(tty0_dev, ascii_char);

                            char echo_c;
                            while (tty_pop_output(tty0_dev, &echo_c))
                            {
                                tty_putc_backbuffer(tty0_dev, echo_c);
                            }
                        }
                    }
                }
                else
                {
                    // Se nenhuma PTY estiver ativa no sistema, mantém o comportamento original do hardware
                    tty_push_char_isr(tty_focado, ascii_char);

                    char echo_c;
                    while (tty_pop_output(tty_focado, &echo_c))
                    {
                        tty_putc_backbuffer(tty_focado, echo_c);
                    }
                }
            }
        }
    }
}