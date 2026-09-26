#include <math.h>
#include <assert.h>

#define PI 3.14159265358979323846

// Aproximação de arctangente usando a série de Taylor para |x| ≤ 1
double atan_approx(double x) {
    // Para valores fora de [-1,1], usa identidade
    if (x > 1.0) return PI / 2 - atan_approx(1.0 / x);
    if (x < -1.0) return -PI / 2 - atan_approx(1.0 / x);

    double term = x;
    double result = term;
    double x2 = x * x;
    int i = 3;
    int sign = -1;
    while (i < 20) {
        term *= x2;
        result += sign * (term / i);
        i += 2;
        sign = -sign;
    }
    return result;
}

// Raiz quadrada simples via método de Newton-Raphson
double sqrt_approx(double x) {
    if (x < 0) return 0; // ou assert(false)
    double guess = x > 1 ? x : 1;
    for (int i = 0; i < 10; ++i)
        guess = 0.5 * (guess + x / guess);
    return guess;
}

double acos(double x)
{
    assert(x >= -1.0 && x <= 1.0);

    if (x == 1.0) return 0.0;
    if (x == -1.0) return PI;

    double ratio = sqrt_approx((1 - x) / (1 + x));
    return 2 * atan_approx(ratio);
}
