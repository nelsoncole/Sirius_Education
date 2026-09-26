#include <math.h>

double floor(double x)
{
    if ((int)x == x) {
        return x;  // já é inteiro
    }

    if (x >= 0) {
        return (int)x;
    } else {
        return (int)x - 1;
    }
}
