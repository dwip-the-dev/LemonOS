#ifndef POSIX_TIME_H
#define POSIX_TIME_H

#include <stdint.h>
#include <stddef.h>

typedef int64_t time_t;
typedef int32_t suseconds_t;

struct tm {
    int tm_sec;    // Seconds [0, 60]
    int tm_min;    // Minutes [0, 59]
    int tm_hour;   // Hour [0, 23]
    int tm_mday;   // Day of month [1, 31]
    int tm_mon;    // Month [0, 11]
    int tm_year;   // Years since 1900
    int tm_wday;   // Day of week [0, 6] (Sunday = 0)
    int tm_yday;   // Day in year [0, 365]
    int tm_isdst;  // Daylight saving flag
};

struct timeval {
    time_t      tv_sec;   // Seconds
    suseconds_t tv_usec;  // Microseconds
};

struct timespec {
    time_t tv_sec;        // Seconds
    long   tv_nsec;       // Nanoseconds
};

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

typedef struct {
    char name[16];
    int  offset_minutes; // Offset from UTC in minutes (e.g. +330 for IST, -300 for EST)
    int  has_dst;
} timezone_info_t;

void             posix_time_init(void);
time_t           posix_time(time_t* tloc);
int              posix_gettimeofday(struct timeval* tv, void* tz);
int              posix_clock_gettime(int clk_id, struct timespec* tp);

struct tm*       posix_gmtime_r(const time_t* timep, struct tm* result);
struct tm*       posix_localtime_r(const time_t* timep, struct tm* result);
time_t           posix_mktime(struct tm* tm);
size_t           posix_strftime(char* s, size_t max, const char* format, const struct tm* tm);

void             posix_tzset(const char* tz_name);
const char*      posix_get_tz_name(void);
int              posix_get_tz_offset_minutes(void);

// High-Resolution Monotonic Hardware Timer API (PIT/RDTSC)
uint32_t         timer_get_ticks_ms(void);
uint64_t         timer_get_ticks_us(void);
uint32_t         timer_get_cycles_per_ms(void);
void             timer_sleep_ms(uint32_t ms);

// Self-test suite
void             posix_time_run_tests(void);

#endif
