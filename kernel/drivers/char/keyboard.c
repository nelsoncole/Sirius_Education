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

/* Estado volátil de controle do fluxo de bytes do barramento */
static int g_is_e0_extended = 0;

/* Buffer circular de pacotes brutos de 16 bits (Raw Scancodes) */
#define KBD_BUFFER_SIZE 128
static uint16_t g_kbd_raw_buffer[KBD_BUFFER_SIZE];
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
    g_kbd_head = 0;
    g_kbd_tail = 0;

    /* Limpa resíduos de boot */
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUT_BUFFER_FULL)
    {
        inb(KEYBOARD_DATA_PORT);
    }

    kapi_register_irq_handler(IRQ_KEYBOARD, keyboard_handler);
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

    /* Trata respostas ACK do barramento (ignora no buffer de teclas) */
    if (byte == 0xFA) return;

    /* Captura o prefixo de código estendido de 8 bits */
    if (byte == 0xE0)
    {
        g_is_e0_extended = 1;
        return;
    }

    /* Montagem do pacote bruto de 16 bits (Windows Event Style) */
    uint16_t raw_packet = 0;

    /* Identifica se o evento é de liberação (Key Up / Bit 7 ligado) */
    if (byte & 0x80)
    {
        raw_packet |= KEY_RELEASE_FLAG;
        byte &= ~0x80; /* Preserva o scancode de base puro */
    }

    /* Insere o scancode base nos 8 bits inferiores */
    raw_packet |= byte;

    /* Anexa o sinalizador estendido se o byte anterior tiver sido 0xE0 */
    if (g_is_e0_extended)
    {
        raw_packet |= KEY_EXTENDED_FLAG;
        g_is_e0_extended = 0; /* Consome o estado do prefixo */
    }

    /* Injeta o pacote bruto de 16 bits diretamente no buffer circular */
    int next = (g_kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != g_kbd_tail)
    {
        g_kbd_raw_buffer[g_kbd_head] = raw_packet;
        g_kbd_head = next;

        //kprintf("%d", raw_packet);
    }
}

/**
 * Retorna o próximo pacote de scancode bruto de forma bloqueante.
 * Consumido pelas camadas de abstração superiores do sistema.
 */
uint16_t keyboard_get_raw_scancode(void)
{
    while (g_kbd_tail == g_kbd_head)
    {
        __asm__ __volatile__("pause");
    }

    uint16_t packet = g_kbd_raw_buffer[g_kbd_tail];
    g_kbd_tail = (g_kbd_tail + 1) % KBD_BUFFER_SIZE;
    return packet;
}
