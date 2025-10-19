#include "rtc.h"

// RTC ports
#define RTC_ADDRESS 0x70
#define RTC_DATA    0x71

// Simple I/O functions for RTC
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Read from RTC
uint8_t read_rtc(uint8_t reg) {
    outb(RTC_ADDRESS, reg);
    return inb(RTC_DATA);
}

// Check if RTC is in BCD mode
int is_bcd_mode(void) {
    outb(RTC_ADDRESS, RTC_STATUS_B);
    return !(inb(RTC_DATA) & 0x04);
}

// Convert BCD to binary
uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

// Get time from RTC
void get_rtc_time(int* hours, int* minutes, int* seconds) {
    uint8_t last_seconds, last_minutes, last_hours;
    uint8_t seconds_val, minutes_val, hours_val;
    
    do {
        last_seconds = read_rtc(RTC_SECONDS);
        last_minutes = read_rtc(RTC_MINUTES);
        last_hours = read_rtc(RTC_HOURS);
        
        seconds_val = read_rtc(RTC_SECONDS);
        minutes_val = read_rtc(RTC_MINUTES);
        hours_val = read_rtc(RTC_HOURS);
    } while (last_seconds != seconds_val || last_minutes != minutes_val || last_hours != hours_val);
    
    if (is_bcd_mode()) {
        *seconds = bcd_to_bin(seconds_val);
        *minutes = bcd_to_bin(minutes_val);
        *hours = bcd_to_bin(hours_val);
    } else {
        *seconds = seconds_val;
        *minutes = minutes_val;
        *hours = hours_val;
    }
}

// Get date from RTC
void get_rtc_date(int* day, int* month, int* year) {
    uint8_t day_val = read_rtc(RTC_DAY);
    uint8_t month_val = read_rtc(RTC_MONTH);
    uint8_t year_val = read_rtc(RTC_YEAR);
    
    if (is_bcd_mode()) {
        *day = bcd_to_bin(day_val);
        *month = bcd_to_bin(month_val);
        *year = bcd_to_bin(year_val) + 2000;
    } else {
        *day = day_val;
        *month = month_val;
        *year = year_val + 2000;
    }
}
