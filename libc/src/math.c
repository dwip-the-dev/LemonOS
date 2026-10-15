#include <math.h>

double fabs(double x) {
    return x < 0.0 ? -x : x;
}

float fabsf(float x) {
    return x < 0.0f ? -x : x;
}

double floor(double x) {
    long long n = (long long)x;
    if (x < 0.0 && x != (double)n) {
        return (double)(n - 1);
    }
    return (double)n;
}

double ceil(double x) {
    long long n = (long long)x;
    if (x > 0.0 && x != (double)n) {
        return (double)(n + 1);
    }
    return (double)n;
}

double sqrt(double x) {
    if (x < 0.0) return 0.0;
    if (x == 0.0) return 0.0;
    double guess = x / 2.0;
    if (guess < 1.0) guess = 1.0;
    for (int i = 0; i < 20; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    if (y < 0.0) return 1.0 / pow(x, -y);

    long long exp = (long long)y;
    double result = 1.0;
    double base = x;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

double sin(double x) {
    // Normalize to [-PI, PI]
    while (x > M_PI) x -= 2.0 * M_PI;
    while (x < -M_PI) x += 2.0 * M_PI;

    // Taylor series: x - x^3/3! + x^5/5! - x^7/7! + x^9/9!
    double x2 = x * x;
    double term = x;
    double sum = x;

    term *= -x2 / (2.0 * 3.0);
    sum += term;
    term *= -x2 / (4.0 * 5.0);
    sum += term;
    term *= -x2 / (6.0 * 7.0);
    sum += term;
    term *= -x2 / (8.0 * 9.0);
    sum += term;

    return sum;
}

double cos(double x) {
    return sin(x + M_PI / 2.0);
}

double exp(double x) {
    double sum = 1.0;
    double term = 1.0;
    for (int n = 1; n < 25; n++) {
        term *= x / n;
        sum += term;
    }
    return sum;
}

double log(double x) {
    if (x <= 0.0) return 0.0;
    // Natural log using Newton-Raphson approximation
    double y = 0.0;
    for (int i = 0; i < 20; i++) {
        double ey = exp(y);
        y -= (ey - x) / ey;
    }
    return y;
}

// 64-bit integer arithmetic runtime helper routines for freestanding 32-bit x86
#include <stdint.h>

static uint64_t __udivmoddi4(uint64_t num, uint64_t den, uint64_t* rem_p) {
    uint64_t quot = 0, qbit = 1;
    if (den == 0) return 0;
    while ((int64_t)den >= 0 && den < num) {
        den <<= 1;
        qbit <<= 1;
    }
    while (qbit) {
        if (num >= den) {
            num -= den;
            quot |= qbit;
        }
        den >>= 1;
        qbit >>= 1;
    }
    if (rem_p) *rem_p = num;
    return quot;
}

uint64_t __udivdi3(uint64_t a, uint64_t b) {
    return __udivmoddi4(a, b, 0);
}

uint64_t __umoddi3(uint64_t a, uint64_t b) {
    uint64_t rem = 0;
    __udivmoddi4(a, b, &rem);
    return rem;
}

int64_t __divdi3(int64_t a, int64_t b) {
    int neg = 0;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }
    uint64_t res = __udivdi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)res : (int64_t)res;
}

int64_t __moddi3(int64_t a, int64_t b) {
    int neg = 0;
    if (a < 0) { a = -a; neg = 1; }
    if (b < 0) { b = -b; }
    uint64_t res = __umoddi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)res : (int64_t)res;
}

