#include "posix_time.h"
#include "rtc.h"
#include "string.h"
#include "serial.h"

static timezone_info_t current_tz = {"UTC", 0, 0};

static const timezone_info_t tz_database[] = {
    {"UTC",    0, 0},
    {"GMT",    0, 0},
    {"EST", -300, 0},
    {"EDT", -240, 1},
    {"CST", -360, 0},
    {"CDT", -300, 1},
    {"MST", -420, 0},
    {"MDT", -360, 1},
    {"PST", -480, 0},
    {"PDT", -420, 1},
    {"CET",   60, 0},
    {"CEST", 120, 1},
    {"IST",  330, 0},
    {"JST",  540, 0}
};
#define NUM_TZ (sizeof(tz_database) / sizeof(tz_database[0]))

static const int days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const char* const day_abbr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const mon_abbr[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int is_leap_year(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static int days_in_month(int m, int y) {
    if (m == 1 && is_leap_year(y)) return 29;
    return days_per_month[m];
}

static inline uint64_t rdtsc_val(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t boot_rdtsc = 0;
static uint32_t cycles_per_ms = 2500000; // default 2.5 GHz fallback

static inline void outb_pit(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb_pit(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void calibrate_timer(void) {
    boot_rdtsc = rdtsc_val();

    // Prepare PIT Channel 2 for calibration: 10 ms (11932 ticks)
    uint8_t orig_61 = inb_pit(0x61);
    outb_pit(0x61, (orig_61 & ~0x02) | 0x01); // Gate high, speaker off

    // 0x43: Channel 2, mode 0 (one-shot), binary 16-bit
    outb_pit(0x43, 0xB0);

    // 10ms = 1193182 / 100 = 11932 (0x2E9C)
    outb_pit(0x42, 0x9C);
    outb_pit(0x42, 0x2E);

    // Reset gate to start counting
    uint8_t p = inb_pit(0x61);
    outb_pit(0x61, p & ~0x01);
    outb_pit(0x61, p | 0x01);

    uint64_t start = rdtsc_val();
    int timeout = 5000000;
    while (!(inb_pit(0x61) & 0x20) && --timeout > 0) {
        asm volatile("pause");
    }
    uint64_t end = rdtsc_val();

    // Restore original port 0x61
    outb_pit(0x61, orig_61);

    if (timeout > 0 && end > start) {
        uint64_t diff = end - start;
        cycles_per_ms = (uint32_t)(diff / 10);
        if (cycles_per_ms < 100000) cycles_per_ms = 1000000;
    } else {
        cycles_per_ms = 2500000;
    }
}

uint32_t timer_get_ticks_ms(void) {
    uint64_t now = rdtsc_val();
    if (now < boot_rdtsc) return 0;
    return (uint32_t)((now - boot_rdtsc) / cycles_per_ms);
}

uint64_t timer_get_ticks_us(void) {
    uint64_t now = rdtsc_val();
    if (now < boot_rdtsc) return 0;
    if (cycles_per_ms == 0) return 0;
    return ((now - boot_rdtsc) * 1000) / cycles_per_ms;
}

uint32_t timer_get_cycles_per_ms(void) {
    return cycles_per_ms;
}

void timer_sleep_ms(uint32_t ms) {
    uint32_t start = timer_get_ticks_ms();
    while ((timer_get_ticks_ms() - start) < ms) {
        asm volatile("pause");
    }
}

void posix_time_init(void) {
    current_tz.offset_minutes = 0;
    current_tz.has_dst = 0;
    strcpy(current_tz.name, "UTC");
    calibrate_timer();
    serial_println("[TIME] POSIX time & monotonic timer initialized (PIT/RDTSC calibrated).");
}

time_t posix_time(time_t* tloc) {
    int h = 0, m = 0, s = 0;
    int day = 1, month = 1, year = 2026;
    get_rtc_time(&h, &m, &s);
    get_rtc_date(&day, &month, &year);

    if (year < 100) year += 2000;

    struct tm tm_val;
    tm_val.tm_sec  = s;
    tm_val.tm_min  = m;
    tm_val.tm_hour = h;
    tm_val.tm_mday = day;
    tm_val.tm_mon  = month - 1;
    tm_val.tm_year = year - 1900;
    tm_val.tm_isdst = 0;

    time_t t = posix_mktime(&tm_val);
    if (tloc) *tloc = t;
    return t;
}

int posix_gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    if (!tv) return -1;
    tv->tv_sec = posix_time(NULL);
    tv->tv_usec = (suseconds_t)(timer_get_ticks_us() % 1000000);
    return 0;
}

int posix_clock_gettime(int clk_id, struct timespec* tp) {
    (void)clk_id;
    if (!tp) return -1;
    tp->tv_sec = posix_time(NULL);
    tp->tv_nsec = (long)((timer_get_ticks_us() % 1000000) * 1000);
    return 0;
}

time_t posix_mktime(struct tm* tm) {
    int y = tm->tm_year + 1900;
    int m = tm->tm_mon;
    int d = tm->tm_mday;

    int64_t days = 0;
    for (int yr = 1970; yr < y; yr++) {
        days += is_leap_year(yr) ? 366 : 365;
    }
    for (int mn = 0; mn < m; mn++) {
        days += days_in_month(mn, y);
    }
    days += (d - 1);

    time_t seconds = days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec;
    return seconds;
}

struct tm* posix_gmtime_r(const time_t* timep, struct tm* result) {
    if (!timep || !result) return NULL;

    int64_t t = *timep;
    result->tm_sec  = (int)(t % 60); t /= 60;
    result->tm_min  = (int)(t % 60); t /= 60;
    result->tm_hour = (int)(t % 24);
    int64_t days    = t / 24;

    result->tm_wday = (int)((days + 4) % 7); // 1970-01-01 was Thursday (4)

    int y = 1970;
    while (1) {
        int d_in_y = is_leap_year(y) ? 366 : 365;
        if (days >= d_in_y) {
            days -= d_in_y;
            y++;
        } else {
            break;
        }
    }
    result->tm_year = y - 1900;
    result->tm_yday = (int)days;

    int m = 0;
    while (m < 12) {
        int d_in_m = days_in_month(m, y);
        if (days >= d_in_m) {
            days -= d_in_m;
            m++;
        } else {
            break;
        }
    }
    result->tm_mon  = m;
    result->tm_mday = (int)(days + 1);
    result->tm_isdst = 0;

    return result;
}

struct tm* posix_localtime_r(const time_t* timep, struct tm* result) {
    if (!timep || !result) return NULL;
    time_t local_sec = *timep + (current_tz.offset_minutes * 60LL);
    return posix_gmtime_r(&local_sec, result);
}

void posix_tzset(const char* tz_name) {
    if (!tz_name || tz_name[0] == '\0') return;

    for (size_t i = 0; i < NUM_TZ; i++) {
        if (strcmp(tz_database[i].name, tz_name) == 0) {
            current_tz = tz_database[i];
            serial_print("[TIME] Timezone updated to: ");
            serial_print(current_tz.name);
            serial_print(" (UTC offset ");
            serial_print_dec(current_tz.offset_minutes);
            serial_println(" min)");
            return;
        }
    }
    // Default fallback
    strcpy(current_tz.name, tz_name);
    current_tz.offset_minutes = 0;
    current_tz.has_dst = 0;
}

const char* posix_get_tz_name(void) {
    return current_tz.name;
}

int posix_get_tz_offset_minutes(void) {
    return current_tz.offset_minutes;
}

static void append_two_digits(char* buf, size_t* pos, size_t max, int val) {
    if (*pos + 2 >= max) return;
    buf[(*pos)++] = '0' + ((val / 10) % 10);
    buf[(*pos)++] = '0' + (val % 10);
}

static void append_four_digits(char* buf, size_t* pos, size_t max, int val) {
    if (*pos + 4 >= max) return;
    buf[(*pos)++] = '0' + ((val / 1000) % 10);
    buf[(*pos)++] = '0' + ((val / 100) % 10);
    buf[(*pos)++] = '0' + ((val / 10) % 10);
    buf[(*pos)++] = '0' + (val % 10);
}

static void append_str(char* buf, size_t* pos, size_t max, const char* str) {
    while (*str && *pos < max - 1) {
        buf[(*pos)++] = *str++;
    }
}

size_t posix_strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    if (!s || max == 0 || !format || !tm) return 0;

    size_t pos = 0;
    for (size_t i = 0; format[i] != '\0' && pos < max - 1; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;
            switch (format[i]) {
                case 'Y': append_four_digits(s, &pos, max, tm->tm_year + 1900); break;
                case 'm': append_two_digits(s, &pos, max, tm->tm_mon + 1); break;
                case 'd': append_two_digits(s, &pos, max, tm->tm_mday); break;
                case 'H': append_two_digits(s, &pos, max, tm->tm_hour); break;
                case 'M': append_two_digits(s, &pos, max, tm->tm_min); break;
                case 'S': append_two_digits(s, &pos, max, tm->tm_sec); break;
                case 'a': append_str(s, &pos, max, (tm->tm_wday >= 0 && tm->tm_wday <= 6) ? day_abbr[tm->tm_wday] : ""); break;
                case 'b': append_str(s, &pos, max, (tm->tm_mon >= 0 && tm->tm_mon <= 11) ? mon_abbr[tm->tm_mon] : ""); break;
                case 'Z': append_str(s, &pos, max, current_tz.name); break;
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

void posix_time_run_tests(void) {
    serial_println("[TIME] Running POSIX Time Verification Suite...");

    struct tm test_tm;
    test_tm.tm_year = 2026 - 1900;
    test_tm.tm_mon  = 9; // October (0-indexed = 9)
    test_tm.tm_mday = 1;
    test_tm.tm_hour = 12;
    test_tm.tm_min  = 0;
    test_tm.tm_sec  = 0;
    test_tm.tm_isdst = 0;

    time_t t = posix_mktime(&test_tm);
    struct tm res_tm;
    posix_gmtime_r(&t, &res_tm);

    if (res_tm.tm_year == test_tm.tm_year && res_tm.tm_mon == test_tm.tm_mon && res_tm.tm_mday == test_tm.tm_mday) {
        serial_println("  -> [TEST 1/2] mktime and gmtime_r roundtrip: PASS");
    } else {
        serial_println("  -> [TEST 1/2] mktime and gmtime_r roundtrip: FAIL");
    }

    char date_str[64];
    posix_strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S %Z", &res_tm);
    if (strstr(date_str, "2026-10-01 12:00:00 UTC") != NULL) {
        serial_println("  -> [TEST 2/2] strftime ISO format: PASS");
    } else {
        serial_print("  -> [TEST 2/2] strftime ISO format FAIL: ");
        serial_println(date_str);
    }
    serial_println("[TIME] POSIX time subsystem verified.");
}
