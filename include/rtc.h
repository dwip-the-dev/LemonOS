#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// RTC registers
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_B    0x0B

void get_rtc_time(int* hours, int* minutes, int* seconds);
void get_rtc_date(int* day, int* month, int* year);
uint8_t read_rtc(uint8_t reg);

#endif
