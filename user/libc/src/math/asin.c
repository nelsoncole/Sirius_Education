#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <errno.h>

// Aproximação de asin baseada em fórmula racional para |x| < 0.5
static double asin_poly(double x) {
    const double p0 = 1.66666666666666657415e-1;
    const double p1 = 7.4953002686e-2;
    const double p2 = 4.5470025998e-2;
    const double p3 = 2.4179514512e-2;
    const double p4 = 4.2166308807e-2;

    double x2 = x * x;
    return (((p4 * x2 + p3) * x2 + p2) * x2 + p1) * x2 + p0;
}

double asin(double x) {
    if (x > 1.0 || x < -1.0) {
        errno = EDOM;
        return NAN;
    }

    if (x == 1.0) return M_PI_2;       // π/2
    if (x == -1.0) return -M_PI_2;     // -π/2
    if (x == 0.0) return 0.0;

    double absx = fabs(x);

    // Para |x| < 0.5, usamos expansão racional direta
    if (absx < 0.5) {
        return x + x * asin_poly(x);
    }

    // Para |x| >= 0.5, usamos transformação trigonométrica:
    // asin(x) = π/2 - 2 * asin(sqrt((1 - x)/2))   (para x > 0)
    // ou       = -π/2 + 2 * asin(sqrt((1 + x)/2)) (para x < 0)

    double z = sqrt(0.5 * (1.0 - absx));
    double approx = z + z * asin_poly(z);

    double result = M_PI_2 - 2.0 * approx;
    return (x < 0.0) ? -result : result;
}
