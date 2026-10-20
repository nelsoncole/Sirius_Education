/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ioctl.h (sys/ioctl.h)
 *    Description: Cabeçalho da biblioteca C padrão (Libc) para chamadas de
 *                 controlo de dispositivos de Entrada/Saída e Terminais (PTY/TTY).
 *                 Compatível com as syscalls de Ring 3 do Sirius OS.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 17/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _IOCTL_H
#define _IOCTL_H

/* Comando Mágico customizado do Sirius OS para diagnóstico do VFS */
#define VFS_CMD_PRINT_TREE    0x1001

/* Comandos de controlo para Pseudo-Terminais (PTY) e Terminais (TTY) */
#define TIOCGPTN              0x5430  /* Obtém o número/ID da PTY escrava (Get PTY Number) */
#define TIOCSPTLCK            0x40045431 /* Tranca/Destranca o nó da PTY escrava (Set PTY Lock) */

/* Comandos adicionais de controlo de janela (Útil para o encaminhamento do SSH) */
#define TIOCGWINSZ            0x5413  /* Obtém o tamanho da janela (Get Window Size) */
#define TIOCSWINSZ            0x5414  /* Define o tamanho da janela (Set Window Size) */

/* Estrutura padrão para redimensionamento de terminal */
struct winsize {
    unsigned short ws_row;     /* Número de linhas */
    unsigned short ws_col;     /* Número de colunas */
    unsigned short ws_xpixel;  /* Largura em pixels (opcional) */
    unsigned short ws_ypixel;  /* Altura em pixels (opcional) */
};

int ioctl(int fd, unsigned long request, ...);

#endif