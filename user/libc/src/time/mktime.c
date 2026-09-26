#include <time.h>

static int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static const int days_per_month[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

time_t mktime(struct tm *t) {
    int year = t->tm_year + 1900;
    time_t days = 0;

    // Dias dos anos anteriores
    for (int y = 1970; y < year; y++)
        days += is_leap(y) ? 366 : 365;

    // Dias dos meses anteriores no mesmo ano
    for (int m = 0; m < t->tm_mon; m++)
        days += days_per_month[m] + (m == 1 && is_leap(year) ? 1 : 0);

    // Dias do mês
    days += t->tm_mday - 1;

    time_t seconds = days * 86400L;
    seconds += t->tm_hour * 3600L;
    seconds += t->tm_min * 60L;
    seconds += t->tm_sec;

    return seconds;
}
