#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios *term)
{
    if (!term) {
        errno = EINVAL;
        return -1;
    }

    return ioctl(fd, TCGETS, term);
}

int tcsetattr(int fd, int optional_actions, const struct termios *term)
{
    unsigned long request;

    if (!term) {
        errno = EINVAL;
        return -1;
    }

    switch (optional_actions) {
        case TCSANOW:
            request = TCSETS;
            break;

        case TCSADRAIN:
            request = TCSETSW;
            break;

        case TCSAFLUSH:
            request = TCSETSF;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    return ioctl(fd, request, term);
}

speed_t cfgetispeed(const struct termios *term)
{
    if (!term) {
        errno = EINVAL;
        return (speed_t)-1;
    }

    if (term->c_ispeed != B0)
        return term->c_ispeed;

    return cfgetospeed(term);
}

speed_t cfgetospeed(const struct termios *term)
{
    if (!term) {
        errno = EINVAL;
        return (speed_t)-1;
    }

    if (term->c_ospeed != B0)
        return term->c_ospeed;

    return term->c_cflag & CBAUD;
}

int cfsetispeed(struct termios *term, speed_t speed)
{
    if (!term) {
        errno = EINVAL;
        return -1;
    }

    if (speed != B0 && (speed & ~CBAUD) != 0) {
        errno = EINVAL;
        return -1;
    }

    term->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios *term, speed_t speed)
{
    if (!term) {
        errno = EINVAL;
        return -1;
    }

    if ((speed & ~CBAUD) != 0) {
        errno = EINVAL;
        return -1;
    }

    term->c_ospeed = speed;
    term->c_cflag &= ~CBAUD;
    term->c_cflag |= speed;

    return 0;
}

void cfmakeraw(struct termios *term)
{
    if (!term)
        return;

    term->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                       INLCR | IGNCR | ICRNL | IXON);

    term->c_oflag &= ~OPOST;

    term->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    term->c_cflag &= ~(CSIZE | PARENB);
    term->c_cflag |= CS8;

    term->c_cc[VMIN] = 1;
    term->c_cc[VTIME] = 0;
}