#include <math.h>

double tanh(double x) {
    double ex = exp(x);
    double enx = exp(-x);
    return (ex - enx) / (ex + enx);
}
