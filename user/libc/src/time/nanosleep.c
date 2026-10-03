#include <time.h>

int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    if (!req) return -1;

    // Abstração didática temporária baseada em ciclos se não houver timer de hardware
    // No futuro, deve invocar: syscall2(SYS_NANOSLEEP, (uint64_t)req, (uint64_t)rem);
    volatile long ciclos = req->tv_sec * 100000000 + (req->tv_nsec / 10);
    while (ciclos > 0) {
        __asm__ __volatile__("pause");
        ciclos--;
    }
    return 0;
}
