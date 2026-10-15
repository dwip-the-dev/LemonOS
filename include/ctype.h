#ifndef CTYPE_H
#define CTYPE_H

static inline int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

static inline int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static inline int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

static inline int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

static inline int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static inline int isprint(int c) {
    return (c >= 32 && c <= 126);
}

static inline int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// In case any external code uses glibc ctype tables
extern const unsigned short *__ctype_b_loc(void);
extern const int *__ctype_toupper_loc(void);
extern const int *__ctype_tolower_loc(void);

#endif // CTYPE_H
