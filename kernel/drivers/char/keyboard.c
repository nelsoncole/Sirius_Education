/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: keyboard.c
 *    Description: Implementação do driver do teclado PS/2 em modo Raw (Windows style).
 *                 Captura e armazena estritamente os scancodes de hardware e 
 *                 as suas flags de transição (Press/Release/Extended) sem conversão.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/char/keyboard.h>
#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/klib.h>

#define IRQ_KEYBOARD 1

/* Definições de Scancodes de controle no Set 1 */
#define SCAN_LSHIFT    0x2A
#define SCAN_RSHIFT    0x36
#define SCAN_CAPSLOCK  0x3A

/* Estado volátil de controle do fluxo de bytes do barramento */
static int g_is_e0_extended = 0;
static uint8_t g_current_modifiers = 0; // Guarda o estado vivo do Shift/Caps

/* Buffer circular de pacotes brutos de 16 bits (Raw Scancodes) */
static kbd_event_t g_kbd_raw_buffer[KBD_BUFFER_SIZE];
static int g_kbd_head = 0;
static int g_kbd_tail = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void keyboard_ps2_init(void)
{
    kprintf("[Teclado] Inicializando barramento PS/2 em modo RAW (Windows Model)...\n");
    g_is_e0_extended = 0;
    g_current_modifiers = 0;
    g_kbd_head = 0;
    g_kbd_tail = 0;

    /* Limpa resíduos de boot */
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUT_BUFFER_FULL)
    {
        inb(KEYBOARD_DATA_PORT);
    }

    irq_handler(IRQ_KEYBOARD, keyboard_handler);
}

/**
 * Rotina de interrupção ISR do Teclado.
 * Captura os estados lógicos do hardware e monta o pacote bruto de 16 bits.
 */
__attribute__((force_align_arg_pointer))
void keyboard_handler(void)
{
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if (!(status & KEYBOARD_STATUS_OUT_BUFFER_FULL)) return;

    uint8_t byte = inb(KEYBOARD_DATA_PORT);
    if (byte == 0xFA) return;

    if (byte == 0xE0) {
        g_is_e0_extended = 1;
        return;
    }

    /* 
     * LÓGICA DE ATUALIZAÇÃO DO ESTADO VIVO DOS MODIFICADORES
     * Feito imediatamente na interrupção para manter precisão de tempo real.
     */
    uint8_t packet_modifiers = g_current_modifiers;

    if (byte & 0x80) {
        /* CASO A: A tecla foi Solta (Key Up) */
        packet_modifiers |= KBD_MOD_RELEASE; // Sinaliza no pacote
        uint8_t pure_scancode = byte & ~0x80;

        if (pure_scancode == SCAN_LSHIFT)  g_current_modifiers &= ~KBD_MOD_LSHIFT;
        if (pure_scancode == SCAN_RSHIFT)  g_current_modifiers &= ~KBD_MOD_RSHIFT;
        
        byte = pure_scancode; // Preserva o scancode puro para inserção
    } 
    else {
        /* CASO B: A tecla foi Pressionada (Key Down) */
        if (byte == SCAN_LSHIFT)   g_current_modifiers |= KBD_MOD_LSHIFT;
        if (byte == SCAN_RSHIFT)   g_current_modifiers |= KBD_MOD_RSHIFT;
        if (byte == SCAN_CAPSLOCK) g_current_modifiers ^= KBD_MOD_CAPS; // Inverte (Toggle)

        packet_modifiers = g_current_modifiers; // Atualiza o snapshot do pacote
    }

    g_is_e0_extended = 0; // Consome o prefixo temporário nesta fase simples

    /* Injeta a estrutura montada no buffer circular */
    int next = (g_kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != g_kbd_tail) {
        g_kbd_raw_buffer[g_kbd_head].scancode = byte;
        g_kbd_raw_buffer[g_kbd_head].modifiers_state = packet_modifiers;
        g_kbd_head = next;
    }
}

/**
 * Retorna a estrutura completa do evento de tecla de forma bloqueante.
 */
kbd_event_t keyboard_get_event(void)
{
    while (g_kbd_tail == g_kbd_head) {
        __asm__ __volatile__("pause");
    }

    kbd_event_t event = g_kbd_raw_buffer[g_kbd_tail];
    g_kbd_tail = (g_kbd_tail + 1) % KBD_BUFFER_SIZE;
    return event;
}
