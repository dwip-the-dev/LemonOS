#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include "syscall_nums.h"

time_t time(time_t* tloc) {
    time_t t = (time_t)__syscall0(SYS_TIME);
    if (tloc) *tloc = t;
    return t;
}

int gettimeofday(struct timeval* tv, struct timezone* tz) {
    (void)tz;
    return __syscall_ret(__syscall2(SYS_GETTIMEOFDAY, (unsigned int)tv, (unsigned int)tz));
}

int settimeofday(const struct timeval* tv, const struct timezone* tz) {
    (void)tv; (void)tz;
    return 0;
}

clock_t clock(void) {
    return (clock_t)__syscall0(68); // SYS_GET_TICKS_MS
}

double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;
    return __syscall_ret(__syscall1(SYS_NANOSLEEP, (unsigned int)req));
}

static struct tm static_tm;

struct tm* gmtime(const time_t* timep) {
    if (!timep) return NULL;
    time_t t = *timep;

    static_tm.tm_sec = t % 60;
    t /= 60;
    static_tm.tm_min = t % 60;
    t /= 60;
    static_tm.tm_hour = t % 24;
    t /= 24;

    static_tm.tm_wday = (t + 4) % 7; // Epoch was Thursday

    // Year calculation (simplified leap year estimation)
    int year = 1970;
    while (1) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        int days_in_year = leap ? 366 : 365;
        if (t >= (time_t)days_in_year) {
            t -= days_in_year;
            year++;
        } else {
            break;
        }
    }
    static_tm.tm_year = year - 1900;
    static_tm.tm_yday = (int)t;

    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    int mdays[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int mon = 0;
    while (mon < 12 && t >= (time_t)mdays[mon]) {
        t -= mdays[mon];
        mon++;
    }
    static_tm.tm_mon = mon;
    static_tm.tm_mday = (int)t + 1;
    static_tm.tm_isdst = 0;

    return &static_tm;
}

struct tm* localtime(const time_t* timep) {
    return gmtime(timep);
}

static char ascbuf[32];
char* asctime(const struct tm* tm) {
    if (!tm) return NULL;
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    snprintf(ascbuf, sizeof(ascbuf), "%.3s %.3s %2d %02d:%02d:%02d %d\n",
             days[tm->tm_wday % 7], months[tm->tm_mon % 12],
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
             1900 + tm->tm_year);
    return ascbuf;
}

char* ctime(const time_t* timep) {
    return asctime(localtime(timep));
}

static void append_num2(char* buf, size_t* pos, size_t max, int val) {
    if (*pos + 2 >= max) return;
    buf[(*pos)++] = '0' + ((val / 10) % 10);
    buf[(*pos)++] = '0' + (val % 10);
}

static void append_num4(char* buf, size_t* pos, size_t max, int val) {
    if (*pos + 4 >= max) return;
    buf[(*pos)++] = '0' + ((val / 1000) % 10);
    buf[(*pos)++] = '0' + ((val / 100) % 10);
    buf[(*pos)++] = '0' + ((val / 10) % 10);
    buf[(*pos)++] = '0' + (val % 10);
}

static void append_s(char* buf, size_t* pos, size_t max, const char* str) {
    while (*str && *pos < max - 1) {
        buf[(*pos)++] = *str++;
    }
}

size_t strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    if (!s || max == 0 || !format || !tm) return 0;

    static const char* const day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char* const day_abbrs[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const mon_names[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    static const char* const mon_abbrs[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    size_t pos = 0;
    for (size_t i = 0; format[i] != '\0' && pos < max - 1; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;
            switch (format[i]) {
                case 'Y': append_num4(s, &pos, max, tm->tm_year + 1900); break;
                case 'y': append_num2(s, &pos, max, tm->tm_year % 100); break;
                case 'm': append_num2(s, &pos, max, tm->tm_mon + 1); break;
                case 'd': append_num2(s, &pos, max, tm->tm_mday); break;
                case 'e': {
                    if (pos < max - 1) {
                        if (tm->tm_mday < 10) s[pos++] = ' ';
                        else s[pos++] = '0' + (tm->tm_mday / 10);
                        if (pos < max - 1) s[pos++] = '0' + (tm->tm_mday % 10);
                    }
                    break;
                }
                case 'H': append_num2(s, &pos, max, tm->tm_hour); break;
                case 'I': append_num2(s, &pos, max, (tm->tm_hour % 12 == 0) ? 12 : (tm->tm_hour % 12)); break;
                case 'M': append_num2(s, &pos, max, tm->tm_min); break;
                case 'S': append_num2(s, &pos, max, tm->tm_sec); break;
                case 'p': append_s(s, &pos, max, (tm->tm_hour >= 12) ? "PM" : "AM"); break;
                case 'P': append_s(s, &pos, max, (tm->tm_hour >= 12) ? "pm" : "am"); break;
                case 'a': append_s(s, &pos, max, (tm->tm_wday >= 0 && tm->tm_wday <= 6) ? day_abbrs[tm->tm_wday] : ""); break;
                case 'A': append_s(s, &pos, max, (tm->tm_wday >= 0 && tm->tm_wday <= 6) ? day_names[tm->tm_wday] : ""); break;
                case 'b':
                case 'h': append_s(s, &pos, max, (tm->tm_mon >= 0 && tm->tm_mon <= 11) ? mon_abbrs[tm->tm_mon] : ""); break;
                case 'B': append_s(s, &pos, max, (tm->tm_mon >= 0 && tm->tm_mon <= 11) ? mon_names[tm->tm_mon] : ""); break;
                case 'F':
                    append_num4(s, &pos, max, tm->tm_year + 1900);
                    if (pos < max - 1) s[pos++] = '-';
                    append_num2(s, &pos, max, tm->tm_mon + 1);
                    if (pos < max - 1) s[pos++] = '-';
                    append_num2(s, &pos, max, tm->tm_mday);
                    break;
                case 'T':
                    append_num2(s, &pos, max, tm->tm_hour);
                    if (pos < max - 1) s[pos++] = ':';
                    append_num2(s, &pos, max, tm->tm_min);
                    if (pos < max - 1) s[pos++] = ':';
                    append_num2(s, &pos, max, tm->tm_sec);
                    break;
                case 'R':
                    append_num2(s, &pos, max, tm->tm_hour);
                    if (pos < max - 1) s[pos++] = ':';
                    append_num2(s, &pos, max, tm->tm_min);
                    break;
                case 'Z': append_s(s, &pos, max, "UTC"); break;
                case '%': if (pos < max - 1) s[pos++] = '%'; break;
                default:  if (pos < max - 1) s[pos++] = format[i]; break;
            }
        } else {
            s[pos++] = format[i];
        }
    }
    s[pos] = '\0';
    return pos;
}

int clock_gettime(clockid_t clk_id, struct timespec* tp) {
    (void)clk_id;
    if (!tp) {
        errno = EINVAL;
        return -1;
    }
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return -1;
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = (long)tv.tv_usec * 1000L;
    return 0;
}
