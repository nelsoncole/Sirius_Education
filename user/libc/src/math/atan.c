#include <math.h>
#include <errno.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double atan(double x) {
    if (x > 1.0)
        return M_PI / 2 - atan(1.0 / x);
    else if (x < -1.0)
        return -M_PI / 2 - atan(1.0 / x);

    // Série de Taylor para |x| <= 1
    // atan(x) = x - x^3/3 + x^5/5 - x^7/7 + ...
    double x2 = x * x;
    double term = x;
    double result = x;
    double sign = -1.0;

    for (int n = 3; n <= 25; n += 2) {
        term *= x2;
        result += sign * term / n;
        sign = -sign;
    }

    return result;
}
