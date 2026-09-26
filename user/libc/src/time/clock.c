#include <time.h>
#include <stdint.h>

clock_t *__ticks;

clock_t clock(void)
{
    return (clock_t)*__ticks;
}
