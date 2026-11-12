/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mouse.h
 *    Description: Cabeçalho do controlador de rato PS/2 padrão.
 *                 Define os comandos do controlador 8042 e a estrutura
 *                 de pacotes de dados brutos (Raw Mouse Packets).
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

#ifndef _MOUSE_H_
#define _MOUSE_H_

#include <kernel/lib/stdint.h>

#define MOUSE_DATA_PORT 0x60
#define MOUSE_STATUS_PORT 0x64

/* Comandos de controlo do chip 8042 e Rato */
#define MOUSE_CMD_WRITE_NEXT 0xD4  /* Avisa o 8042 para encaminhar o próximo byte para o rato */
#define MOUSE_CMD_ENABLE_REPT 0xF4 /* Comando enviado ao rato para ativar o envio de pacotes */

/* Sinalizadores de Estado do Primeiro Byte do Pacote */
#define MOUSE_BTN_LEFT (1 << 0)   /* Bit 0: Botão Esquerdo Pressionado */
#define MOUSE_BTN_RIGHT (1 << 1)  /* Bit 1: Botão Direito Pressionado */
#define MOUSE_BTN_MIDDLE (1 << 2) /* Bit 2: Botão do Meio Pressionado */
#define MOUSE_X_SIGN (1 << 4)     /* Bit 4: Sinal de Delta X (1 = Negativo) */
#define MOUSE_Y_SIGN (1 << 5)     /* Bit 5: Sinal de Delta Y (1 = Negativo) */

/**
 * Estrutura unificada de pacotes brutos de movimento e cliques do rato.
 */
typedef struct
{
    uint8_t buttons; /* Bitmask com os estados dos botões */
    int16_t delta_x; /* Deslocamento relativo no eixo X */
    int16_t delta_y; /* Deslocamento relativo no eixo Y */
} mouse_packet_t;

void mouse_ps2_init(void);
void mouse_handler(void);

/* Interface pública para capturar o pacote tratado do buffer */
int mouse_get_packet(mouse_packet_t *packet);

/*
Como a Consola ou a Interface Gráfica vai ler o mouse?

mouse_packet_t raw_data;
static int absolute_cursor_x = 400; // Centro do ecrã fictício
static int absolute_cursor_y = 300;

if (mouse_get_packet(&raw_data)) {
    // Atualiza a posição com base no deslocamento relativo capturado em Ring 0
    absolute_cursor_x += raw_data.delta_x;
    absolute_cursor_y += raw_data.delta_y;

    if (raw_data.buttons & MOUSE_BTN_LEFT) {
        kprintf("[GUI] Clique com o Botão Esquerdo em (%d, %d)\n", absolute_cursor_x, absolute_cursor_y);
    }
}

 */

#endif
