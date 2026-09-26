#include <math.h>
#include <errno.h>

#ifndef M_LN2
    #define M_LN2 0.69314718055994530942  // ln(2)
#endif

double log2(double x) {
    if (x <= 0.0) {
        errno = EDOM;
        return -INFINITY; // ou NAN, dependendo do tratamento desejado
    }

    // Redução de intervalo: converte x para forma x = m * 2^k
    int exponent = 0;
    double mantissa = frexp(x, &exponent);  // x = mantissa * 2^exponent, 0.5 <= mantissa < 1

    // ln(mantissa) via série de Mercator: ln((1 + y)/(1 - y)) = 2(y + y^3/3 + y^5/5 + ...)
    // y = (mantissa - 1) / (mantissa + 1)
    double y = (mantissa - 1) / (mantissa + 1);
    double y2 = y * y;
    double term = y;
    double sum = y;
    for (int n = 3; n <= 25; n += 2) {
        term *= y2;
        sum += term / n;
    }
    double ln_mantissa = 2 * sum;

    return ((double)exponent + ln_mantissa / M_LN2);
}
