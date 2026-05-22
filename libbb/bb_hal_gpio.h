#ifndef BB_HAL_GPIO_H
#define BB_HAL_GPIO_H

// GPIO abstraction over sysfs /sys/class/gpio/
// For kernel < 5.10 where sysfs GPIO is still supported.

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
    int  num;           // GPIO number (global)
    int  value_fd;      // fd for /sys/class/gpio/gpio<N>/value
    int  exported;      // 1 if we exported this GPIO
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

// Close and unexport
void bb_gpio_close(bb_gpio_t *gpio);

#endif
