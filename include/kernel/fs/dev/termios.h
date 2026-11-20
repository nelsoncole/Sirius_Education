#ifndef TERMIOS_H
#define TERMIOS_H

#include <kernel/drivers/tty/tty.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios
{
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

/* Local flags */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100

/* ioctl requests */
#define TCGETS   0x5401
#define TCSETS   0x5402
#define TCSETSW  0x5403
#define TCSETSF  0x5404

static inline uint32_t termios_to_tty_lflag(uint32_t flags)
{
    uint32_t result = 0;

    if (flags & ICANON)
        result |= TTY_ICANON;

    if (flags & ECHO)
        result |= TTY_ECHO;

    return result;
}

static inline uint32_t tty_to_termios_lflag(uint32_t flags)
{
    uint32_t result = 0;

    if (flags & TTY_ICANON)
        result |= ICANON;

    if (flags & TTY_ECHO)
        result |= ECHO;

    return result;
}

#endif