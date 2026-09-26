#include <time.h>

static const int days_per_month[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};
// Verifica se um ano é bissexto
static int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

struct tm *gmtime(const time_t *timer) {
    static struct tm t;
    time_t secs = *timer;
    int year = 1970;

    t.tm_sec = secs % 60;
    secs /= 60;
    t.tm_min = secs % 60;
    secs /= 60;
    t.tm_hour = secs % 24;
    secs /= 24;

    // Dia da semana (1970-01-01 = quinta-feira = 4)
    t.tm_wday = (secs + 4) % 7;

    // Conta anos completos
    while (1) {
        int ydays = is_leap(year) ? 366 : 365;
        if (secs >= ydays) {
            secs -= ydays;
            year++;
        } else break;
    }

    t.tm_year = year - 1900;
    t.tm_yday = secs;

    // Calcula mês e dia do mês
    int month = 0;
    while (1) {
        int dim = days_per_month[month];
        if (month == 1 && is_leap(year)) dim++; // fevereiro
        if (secs >= dim) {
            secs -= dim;
            month++;
        } else break;
    }

    t.tm_mon = month;
    t.tm_mday = secs + 1;

    t.tm_isdst = 0;

    return &t;
}

