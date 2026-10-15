#ifndef _TIME_H
#define _TIME_H

#include <sys/types.h>

#define CLOCKS_PER_SEC 1000

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

time_t     time(time_t* tloc);
double     difftime(time_t time1, time_t time0);
clock_t    clock(void);
struct tm* gmtime(const time_t* timep);
struct tm* localtime(const time_t* timep);
char*      asctime(const struct tm* tm);
char*      ctime(const time_t* timep);
size_t     strftime(char* s, size_t max, const char* format, const struct tm* tm);
int        nanosleep(const struct timespec* req, struct timespec* rem);
int        clock_gettime(clockid_t clk_id, struct timespec* tp);

#endif // _TIME_H
