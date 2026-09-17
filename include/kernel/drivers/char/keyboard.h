/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: keyboard.h
 *    Description: Cabeçalho do controlador de teclado PS/2 em modo Raw.
 *                 Define a estrutura de pacotes de scancode padrão Windows.
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

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <kernel/lib/stdint.h>

/* ========================================================================
 * PORTAS DE E/S DO CONTROLADOR PS/2 (Intel 8042)
 * ======================================================================== */
#define KEYBOARD_DATA_PORT    0x60  /* Leitura do Scancode / Escrita de Comandos */
#define KEYBOARD_STATUS_PORT  0x64  /* Leitura do Estado do Controlador */

/* ========================================================================
 * MÁSCARAS DE BITS DO REGISTO DE ESTADO
 * ======================================================================== */
#define KEYBOARD_STATUS_OUT_BUFFER_FULL 0x01 /* Bit 0: Dados prontos para leitura na porta 0x60 */
#define KEYBOARD_STATUS_IN_BUFFER_FULL  0x02 /* Bit 1: Buffer de entrada cheio (não escrever comandos) */

/* Máscaras de Estado de Evento (Padrão Windows) */
#define KEY_RELEASE_FLAG      0x0100  /* Bit ativo indica que a tecla foi solta (Key Up) */
#define KEY_EXTENDED_FLAG     0x0200  /* Bit ativo indica tecla precedida por 0xE0 */

/* Máscaras de bits para o campo 'modifiers_state' */
#define KBD_MOD_LSHIFT   (1 << 0)
#define KBD_MOD_RSHIFT   (1 << 1)
#define KBD_MOD_CAPS     (1 << 2)
#define KBD_MOD_RELEASE  (1 << 7) /* Tecla solta (Key Up) */

/**
 * Estrutura atómica do pacote de teclado guardado no buffer
 */
typedef struct {
    uint8_t  scancode;         /* O código físico base puro (Set 1) */
    uint8_t  modifiers_state;  /* Mapa de bits com o estado atual do teclado */
} kbd_event_t;

#define KBD_BUFFER_SIZE 128

/**
 * Inicializa o controlador PS/2, limpa buffers residuais e prepara 
 * o subsistema para capturar caracteres.
 */
void keyboard_ps2_init(void);

/**
 * Rotina de Serviço de Interrupção (ISR) chamada pelo vetor IRQ1 (INT 0x21).
 * Lê o hardware e processa o caractere de forma assíncrona.
 */
void keyboard_handler(void);

/* Retorna o evento bruto de scancode de 16 bits */
kbd_event_t keyboard_get_event(void);

#endif
