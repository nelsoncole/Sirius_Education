/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: font.h
 *    Description: Cabeçalho do subsistema de tipos de letra (Font Management).
 *                 Define as dimensões fixas dos glifos e exporta a matriz
 *                 bitmap da fonte clássica VGA 8x16 para renderização.
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 28/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __FONT_H__
#define __FONT_H__

#include <kernel/stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* 
 * Tabela estendida com os 128 caracteres ASCII (0 a 127)
 * Mapeamento linear direto na memória.
 */
extern const uint8_t g_font_bitmap[128][16];

#endif
