#include <math.h>

double ldexp(double x, int exp) {
    return x * pow(2.0, exp);
}
