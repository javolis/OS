/* rtc.c - read wall-clock time from the MC146818 CMOS RTC.
 *
 * Values may be BCD and/or 12-hour depending on register B; we read the
 * format flags and normalize to binary 24-hour. To avoid catching a
 * mid-update half-rolled-over time, we wait for the update-in-progress
 * flag to clear, then read twice and accept the reading only when two
 * consecutive reads agree. */
#include <stdint.h>

#include "io.h"
#include "rtc.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_SECOND 0x00
#define RTC_MINUTE 0x02
#define RTC_HOUR 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

#define STATUS_A_UPDATING 0x80
#define STATUS_B_BINARY 0x04   /* values are binary, not BCD */
#define STATUS_B_24HOUR 0x02   /* hour is 24h, not 12h */
#define HOUR_PM 0x80           /* set on the hour byte in 12h PM */

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int updating(void) {
    return cmos_read(RTC_STATUS_A) & STATUS_A_UPDATING;
}

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

static void read_raw(struct rtc_time *t) {
    while (updating())
        ; /* wait out an in-progress update */
    t->second = cmos_read(RTC_SECOND);
    t->minute = cmos_read(RTC_MINUTE);
    t->hour = cmos_read(RTC_HOUR);
    t->day = cmos_read(RTC_DAY);
    t->month = cmos_read(RTC_MONTH);
    t->year = cmos_read(RTC_YEAR);
}

void rtc_read(struct rtc_time *out) {
    struct rtc_time a, b;

    /* Read until two consecutive samples match: guards against a roll-over
     * landing between byte reads. */
    read_raw(&a);
    do {
        b = a;
        read_raw(&a);
    } while (a.second != b.second || a.minute != b.minute ||
             a.hour != b.hour || a.day != b.day || a.month != b.month ||
             a.year != b.year);

    uint8_t regb = cmos_read(RTC_STATUS_B);
    uint8_t raw_hour = a.hour;

    if (!(regb & STATUS_B_BINARY)) {
        a.second = bcd_to_bin(a.second);
        a.minute = bcd_to_bin(a.minute);
        a.day = bcd_to_bin(a.day);
        a.month = bcd_to_bin(a.month);
        a.year = bcd_to_bin(a.year);
        a.hour = bcd_to_bin((uint8_t)(raw_hour & 0x7F)); /* strip PM flag */
    }

    if (!(regb & STATUS_B_24HOUR) && (raw_hour & HOUR_PM)) {
        a.hour = (uint8_t)((a.hour % 12) + 12); /* 12h PM -> 24h */
    } else if (!(regb & STATUS_B_24HOUR) && a.hour == 12) {
        a.hour = 0; /* 12 AM -> 0 */
    }

    out->second = a.second;
    out->minute = a.minute;
    out->hour = a.hour;
    out->day = a.day;
    out->month = a.month;
    out->year = (uint16_t)(2000 + a.year); /* RTC year is two digits */
}
