#define _USE_MATH_DEFINES
#include <math.h>

double atan2(double y, double x) {
    if (x > 0.0) {
        return atan(y / x);
    } else if (x < 0.0) {
        if (y >= 0.0) {
            return atan(y / x) + M_PI;
        } else {
            return atan(y / x) - M_PI;
        }
    } else { // x == 0
        if (y > 0.0) {
            return M_PI / 2;
        } else if (y < 0.0) {
            return -M_PI / 2;
        } else {
            return 0.0; // indefinido, mas retorna 0 por padrão
        }
    }
}
