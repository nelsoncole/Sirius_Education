#include <math.h>
#include <errno.h>

// Constantes
#define PI 3.14159265358979323846
#define PI_OVER_2 1.57079632679489661923

// Redução de x para o intervalo [-PI, PI]
static double reduce_angle(double x) {
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    return x;
}

// Aproximação de sin(x) via série de Taylor
static double taylor_sin(double x) {
    double term = x, sum = x;
    double x2 = x * x;
    for (int i = 1; i <= 10; ++i) {
        term *= -x2 / ((2*i) * (2*i + 1));
        sum += term;
    }
    return sum;
}

// Aproximação de cos(x) via série de Taylor
static double taylor_cos(double x) {
    double term = 1.0, sum = 1.0;
    double x2 = x * x;
    for (int i = 1; i <= 10; ++i) {
        term *= -x2 / ((2*i - 1) * (2*i));
        sum += term;
    }
    return sum;
}

// Função tangente usando sin(x)/cos(x)
double tan(double x) {
    x = reduce_angle(x); // reduz para [-π, π]

    // verifica se x é próximo de π/2 ou -π/2, onde tan(x) → ∞
    if (fabs(fabs(x) - PI_OVER_2) < 1e-8) {
        errno = ERANGE;
        return HUGE_VAL; // ou ±INFINITY
    }

    double s = taylor_sin(x);
    double c = taylor_cos(x);

    return s / c;
}
