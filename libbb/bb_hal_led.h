#ifndef BB_HAL_LED_H
#define BB_HAL_LED_H

// LED abstraction over sysfs /sys/class/leds/<name>/
typedef struct {
    char name[64];
    char brightness_path[128];
    char trigger_path[128];
    int  max_brightness;
} bb_led_t;

// Open LED by name (e.g. "heartbeat")
int bb_led_open(bb_led_t *led, const char *name);

// Write brightness value (0 to max_brightness)
int bb_led_set_brightness(bb_led_t *led, int value);

// Set trigger mode (e.g. "none", "heartbeat", "timer")
int bb_led_set_trigger(bb_led_t *led, const char *trigger);

// Turn LED on (max) or off (0)
int bb_led_on(bb_led_t *led);
int bb_led_off(bb_led_t *led);

#endif
