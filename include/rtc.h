/* rtc.h - CMOS real-time clock. */
#pragma once
#include <stdint.h>

struct rtc_time {
    uint16_t year;  /* full, e.g. 2026 */
    uint8_t month;  /* 1-12 */
    uint8_t day;    /* 1-31 */
    uint8_t hour;   /* 0-23 */
    uint8_t minute; /* 0-59 */
    uint8_t second; /* 0-59 */
};

void rtc_read(struct rtc_time *out);
