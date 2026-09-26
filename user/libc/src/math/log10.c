#include <math.h>

#ifndef M_LN10
#define M_LN10 2.30258509299404568402  // ln(10)
#endif

double log10(double x) {
    if (x <= 0.0) {
        // Tratamento de erro para logaritmo de número não-positivo
        return -INFINITY;  // ou NAN, dependendo do comportamento desejado
    }
    return log(x) / M_LN10;
}
