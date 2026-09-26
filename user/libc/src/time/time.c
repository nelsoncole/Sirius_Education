#include <time.h>

struct tm *__clock;           // fornecido e atualizado pelo RTC

time_t time(time_t *timer) {
    if (!__clock) return (time_t)-1;

    time_t t = mktime(__clock);
    
    if (timer) {
        *timer = t;
    }

    return t;
}