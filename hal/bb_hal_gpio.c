#include "bb_hal_gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/gpio.h>

// ---------------------------------------------------------------------------
// gpiochip (kernel >= 5.10) path
// ---------------------------------------------------------------------------
static int gpio_find_chip(int gpio_num, int *offset_out)
{
    // Scan /dev/gpiochip0..gpiochip7
    for (int i = 0; i < 8; i++) {
        char dev[32];
        snprintf(dev, sizeof(dev), "/dev/gpiochip%d", i);

        int fd = open(dev, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;

        struct gpiochip_info info;
        if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0) {
            // Check if gpio_num falls within this chip's range
            if (gpio_num >= (int)info.lines) {
                gpio_num -= info.lines;
                close(fd);
                continue;
            }
            // Found the chip — gpio_num is the offset within this chip
            *offset_out = gpio_num;
            close(fd);
            return i;
        }
        close(fd);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Legacy sysfs path (kernel < 5.10)
// ---------------------------------------------------------------------------
#define SYSFS_GPIO "/sys/class/gpio"

static int gpio_has_sysfs(void)
{
    struct stat st;
    return (stat(SYSFS_GPIO, &st) == 0);
}

static int gpio_sysfs_open(bb_gpio_t *gpio, int num, bb_gpio_direction_t dir)
{
    gpio->num = num;
    gpio->chip_fd = -1;
    gpio->line_fd = -1;
    gpio->line_offset = 0;
    gpio->is_sysfs = 1;

    // Export the GPIO
    char path[128];
    FILE *f = fopen(SYSFS_GPIO "/export", "w");
    if (!f) return -1;
    fprintf(f, "%d", num);
    fclose(f);
    usleep(100000);

    // Open value file
    snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/value", num);
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        // retry: unexport + re-export
        f = fopen(SYSFS_GPIO "/unexport", "w");
        if (f) { fprintf(f, "%d", num); fclose(f); usleep(50000); }
        f = fopen(SYSFS_GPIO "/export", "w");
        if (f) { fprintf(f, "%d", num); fclose(f); usleep(100000); }
        fd = open(path, O_RDWR);
        if (fd < 0) return -1;
    }
    gpio->line_fd = fd;  // reuse line_fd for value_fd in sysfs mode
    gpio->exported = 1;

    return bb_gpio_set_direction(gpio, dir);
}

static int gpio_sysfs_set_direction(bb_gpio_t *gpio, bb_gpio_direction_t dir)
{
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/direction", gpio->num);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", dir == BB_GPIO_OUT ? "out" : "in");
    fclose(f);
    return 0;
}

static int gpio_sysfs_read(bb_gpio_t *gpio)
{
    char c;
    lseek(gpio->line_fd, 0, SEEK_SET);
    if (read(gpio->line_fd, &c, 1) != 1) return -1;
    return (c == '1') ? 1 : 0;
}

static int gpio_sysfs_write(bb_gpio_t *gpio, int value)
{
    const char *v = value ? "1" : "0";
    return (write(gpio->line_fd, v, 1) > 0) ? 0 : -1;
}

static void gpio_sysfs_close(bb_gpio_t *gpio)
{
    if (gpio->line_fd >= 0) { close(gpio->line_fd); gpio->line_fd = -1; }
    if (gpio->exported) {
        FILE *f = fopen(SYSFS_GPIO "/unexport", "w");
        if (f) { fprintf(f, "%d", gpio->num); fclose(f); }
        gpio->exported = 0;
    }
}

// ---------------------------------------------------------------------------
// gpiochip v2 (kernel >= 5.10) — primary implementation
// ---------------------------------------------------------------------------
static int gpio_v2_open(bb_gpio_t *gpio, int num, bb_gpio_direction_t dir)
{
    int offset;
    int chip = gpio_find_chip(num, &offset);
    if (chip < 0) return -1;

    char dev[32];
    snprintf(dev, sizeof(dev), "/dev/gpiochip%d", chip);

    int chip_fd = open(dev, O_RDONLY | O_CLOEXEC);
    if (chip_fd < 0) return -1;

    // Build line request
    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));
    req.offsets[0] = offset;
    req.num_lines = 1;
    snprintf(req.consumer, sizeof(req.consumer), "bb-hal-gpio");

    // Set direction flags
    if (dir == BB_GPIO_OUT) {
        req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
        req.config.num_attrs = 0;
    } else {
        req.config.flags = GPIO_V2_LINE_FLAG_INPUT;
        req.config.num_attrs = 0;
    }

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        // If line is busy, try checking if it's free
        close(chip_fd);
        return -1;
    }

    gpio->num = num;
    gpio->chip_fd = chip_fd;
    gpio->line_fd = req.fd;
    gpio->line_offset = offset;
    gpio->chip_num = chip;
    gpio->is_sysfs = 0;
    gpio->exported = 0;
    gpio->direction = dir;

    return 0;
}

static int gpio_v2_read(bb_gpio_t *gpio)
{
    struct gpio_v2_line_values vals;
    vals.bits = 0;
    vals.mask = 1;  // bit 0 in the request (we only requested 1 line)

    if (ioctl(gpio->line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals) < 0)
        return -1;

    return (vals.bits & 1) ? 1 : 0;
}

static int gpio_v2_write(bb_gpio_t *gpio, int value)
{
    struct gpio_v2_line_values vals;
    vals.mask = 1;
    vals.bits = value ? 1 : 0;

    if (ioctl(gpio->line_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals) < 0)
        return -1;

    return 0;
}

static void gpio_v2_close(bb_gpio_t *gpio)
{
    if (gpio->line_fd >= 0) { close(gpio->line_fd); gpio->line_fd = -1; }
    if (gpio->chip_fd >= 0) { close(gpio->chip_fd); gpio->chip_fd = -1; }
}

static int gpio_v2_set_edge(bb_gpio_t *gpio, bb_gpio_edge_t edge)
{
    // Close the existing line request
    if (gpio->line_fd >= 0) {
        close(gpio->line_fd);
        gpio->line_fd = -1;
    }

    // Re-open the chip fd if needed
    int chip_fd = gpio->chip_fd;
    if (chip_fd < 0) {
        char dev[32];
        snprintf(dev, sizeof(dev), "/dev/gpiochip%d", gpio->chip_num);
        chip_fd = open(dev, O_RDONLY | O_CLOEXEC);
        if (chip_fd < 0) return -1;
        gpio->chip_fd = chip_fd;
    }

    // Build line request with edge flags
    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));
    req.offsets[0] = gpio->line_offset;
    req.num_lines = 1;
    snprintf(req.consumer, sizeof(req.consumer), "bb-hal-gpio");

    // Direction
    if (gpio->direction == BB_GPIO_OUT) {
        req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    } else {
        req.config.flags = GPIO_V2_LINE_FLAG_INPUT;
    }

    // Edge detection (input only)
    if (edge != BB_GPIO_EDGE_NONE) {
        req.config.flags |= GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME;
        if (edge == BB_GPIO_EDGE_RISING)
            req.config.flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
        else if (edge == BB_GPIO_EDGE_FALLING)
            req.config.flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
        else if (edge == BB_GPIO_EDGE_BOTH)
            req.config.flags |= GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING;
    }

    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0)
        return -1;

    gpio->line_fd = req.fd;
    return 0;
}

// ---------------------------------------------------------------------------
// Public API — auto-selects sysfs or gpiochip based on kernel
// ---------------------------------------------------------------------------
int bb_gpio_open(bb_gpio_t *gpio, int num, bb_gpio_direction_t dir)
{
    memset(gpio, 0, sizeof(*gpio));
    gpio->line_fd = -1;
    gpio->chip_fd = -1;

    // Prefer gpiochip v2 API if available
    if (!gpio_has_sysfs()) {
        return gpio_v2_open(gpio, num, dir);
    }

    // Fall back to sysfs
    return gpio_sysfs_open(gpio, num, dir);
}

int bb_gpio_set_direction(bb_gpio_t *gpio, bb_gpio_direction_t dir)
{
    if (gpio->is_sysfs) return gpio_sysfs_set_direction(gpio, dir);
    // gpiochip v2: direction is fixed at request time. Re-request if needed.
    return (dir == BB_GPIO_OUT) ? 0 : 0;
}

int bb_gpio_read(bb_gpio_t *gpio)
{
    if (gpio->is_sysfs) return gpio_sysfs_read(gpio);
    return gpio_v2_read(gpio);
}

int bb_gpio_write(bb_gpio_t *gpio, int value)
{
    if (gpio->is_sysfs) return gpio_sysfs_write(gpio, value);
    return gpio_v2_write(gpio, value);
}

int bb_gpio_set_edge(bb_gpio_t *gpio, bb_gpio_edge_t edge)
{
    if (gpio->is_sysfs) {
        char path[128];
        snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/edge", gpio->num);
        FILE *f = fopen(path, "w");
        if (!f) return -1;
        const char *e[] = {"none", "rising", "falling", "both"};
        fprintf(f, "%s", e[edge]);
        fclose(f);
        return 0;
    }
    return gpio_v2_set_edge(gpio, edge);
}

int bb_gpio_poll(bb_gpio_t *gpio, int timeout_ms)
{
    if (gpio->line_fd < 0) return -1;

    if (gpio->is_sysfs) {
        struct pollfd pfd = { .fd = gpio->line_fd, .events = POLLPRI | POLLERR };
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret > 0) {
            char c;
            lseek(gpio->line_fd, 0, SEEK_SET);
            read(gpio->line_fd, &c, 1);
            return bb_gpio_read(gpio);
        }
        return ret;
    }

    // gpiochip v2: poll on line_fd for edge events
    struct pollfd pfd = { .fd = gpio->line_fd, .events = POLLIN | POLLPRI };
    return poll(&pfd, 1, timeout_ms);
}

void bb_gpio_close(bb_gpio_t *gpio)
{
    if (gpio->is_sysfs) {
        gpio_sysfs_close(gpio);
        return;
    }
    gpio_v2_close(gpio);
}
