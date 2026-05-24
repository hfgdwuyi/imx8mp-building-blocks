#ifndef BB_HAL_WDG_H
#define BB_HAL_WDG_H

// Watchdog abstraction over /dev/watchdogX (ioctl)

typedef struct {
    int fd;
    int timeout;   // current timeout in seconds
} bb_wdg_t;

// Open watchdog device (e.g. "/dev/watchdog0")
int  bb_wdg_open(bb_wdg_t *wdg, const char *device);

// Set timeout in seconds. Returns 0 on success.
int  bb_wdg_set_timeout(bb_wdg_t *wdg, int seconds);

// Get current timeout
int  bb_wdg_get_timeout(bb_wdg_t *wdg);

// Kick (pet) the watchdog to prevent reset
int  bb_wdg_kick(bb_wdg_t *wdg);

// Close (will trigger reboot if not kicked before!)
void bb_wdg_close(bb_wdg_t *wdg);

#endif
