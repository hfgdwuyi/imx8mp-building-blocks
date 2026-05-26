#ifndef BB_HAL_GPIO_H
#define BB_HAL_GPIO_H

// GPIO abstraction using Linux gpiochip character device (/dev/gpiochipX)
// Compatible with kernel >= 5.10 (replaces deprecated sysfs GPIO).
// Falls back to sysfs on kernels where /sys/class/gpio exists.

typedef enum {
    BB_GPIO_IN  = 0,
    BB_GPIO_OUT = 1,
} bb_gpio_direction_t;

typedef enum {
    BB_GPIO_EDGE_NONE    = 0,
    BB_GPIO_EDGE_RISING  = 1,
    BB_GPIO_EDGE_FALLING = 2,
    BB_GPIO_EDGE_BOTH    = 3,
} bb_gpio_edge_t;

typedef struct {
    int  num;           // Global GPIO number
    int  chip_fd;       // fd for /dev/gpiochipX
    int  line_fd;       // fd for requested line (from GPIO_V2_GET_LINE_IOCTL)
    int  line_offset;   // offset within the chip
    int  is_sysfs;      // 1 if using legacy sysfs, 0 for gpiochip
    int  exported;      // 1 if exported (sysfs mode only)
} bb_gpio_t;

// Export and configure a GPIO pin. Returns 0 on success.
int  bb_gpio_open(bb_gpio_t *gpio, int num, bb_gpio_direction_t dir);

// Set direction (in/out)
int  bb_gpio_set_direction(bb_gpio_t *gpio, bb_gpio_direction_t dir);

// Read value (0 or 1). Returns -1 on error.
int  bb_gpio_read(bb_gpio_t *gpio);

// Write value (0 or 1)
int  bb_gpio_write(bb_gpio_t *gpio, int value);

// Set edge trigger for interrupt/poll (requires direction=in)
int  bb_gpio_set_edge(bb_gpio_t *gpio, bb_gpio_edge_t edge);

// Wait for edge event (blocking poll on value_fd)
int  bb_gpio_poll(bb_gpio_t *gpio, int timeout_ms);

// Close and release
void bb_gpio_close(bb_gpio_t *gpio);

#endif // BB_HAL_GPIO_H
