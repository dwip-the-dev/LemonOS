#ifndef MATH_H
#define MATH_H

static inline double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

static inline float fabsf(float x) {
    return (x < 0.0f) ? -x : x;
}

static inline double floor(double x) {
    long long i = (long long)x;
    return (x < (double)i) ? (double)(i - 1) : (double)i;
}

static inline double ceil(double x) {
    long long i = (long long)x;
    return (x > (double)i) ? (double)(i + 1) : (double)i;
}

static inline double sqrt(double x) {
    if (x <= 0.0) return 0.0;
    double res = x;
    for (int i = 0; i < 20; i++) {
        res = 0.5 * (res + x / res);
    }
    return res;
}

#endif // MATH_H
