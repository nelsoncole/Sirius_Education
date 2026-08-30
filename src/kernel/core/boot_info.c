/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: boot_info.c
 *    Description: Instanciação e gestão do descritor de informações de boot
 *                 (BOOT_INFO), consolidando os dados passados pelo bootloader.
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 30/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/boot_info.h>

/*
 * PONTEIRO GLOBAL DE INFORMAÇÕES DE BOOT (BOOT_INFO)
 * ------------------------------------------------------------------------
 * Armazena a referência para a estrutura de dados inicializada pelo bootloader.
 * Fornece ao Kernel os parâmetros vitais do sistema, tais como o mapa de 
 * memória física, localização dos módulos e os descritores do Framebuffer.
 */
BOOT_INFO *g_boot_info = (void*)0;
