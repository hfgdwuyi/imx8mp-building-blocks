#ifndef BB_HAL_RTC_H
#define BB_HAL_RTC_H
#include <stdint.h>

// RTC abstraction over /dev/rtc0 (ioctl)

typedef struct {
    int fd;
    char device[64];
} bb_rtc_t;

// Date/time struct
typedef struct {
    int year;   // full year, e.g. 2026
    int mon;    // 1-12
    int day;    // 1-31
    int hour;   // 0-23
    int min;    // 0-59
    int sec;    // 0-59
} bb_rtc_time_t;

// Open RTC device (e.g. "/dev/rtc0")
int  bb_rtc_open(bb_rtc_t *rtc, const char *device);

// Read current date/time from RTC
int  bb_rtc_read(bb_rtc_t *rtc, bb_rtc_time_t *t);

// Set RTC date/time
int  bb_rtc_set(bb_rtc_t *rtc, const bb_rtc_time_t *t);

// Close
void bb_rtc_close(bb_rtc_t *rtc);

#endif
