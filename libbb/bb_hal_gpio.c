#include "bb_hal_gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#define SYSFS_GPIO "/sys/class/gpio"

int bb_gpio_open(bb_gpio_t *gpio, int num, bb_gpio_direction_t dir) {
    memset(gpio, 0, sizeof(*gpio));
    gpio->num = num;
    gpio->value_fd = -1;

    // Export the GPIO
    FILE *f = fopen(SYSFS_GPIO "/export", "w");
    if (!f) return -1;
    fprintf(f, "%d", num);
    fclose(f);

    // Wait a bit for sysfs to create the node
    usleep(100000);

    // Open value file
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/value", num);
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        // Try unexport and re-export (might already be exported)
        f = fopen(SYSFS_GPIO "/unexport", "w");
        if (f) { fprintf(f, "%d", num); fclose(f); usleep(50000); }
        f = fopen(SYSFS_GPIO "/export", "w");
        if (f) { fprintf(f, "%d", num); fclose(f); usleep(100000); }
        fd = open(path, O_RDWR);
        if (fd < 0) return -1;
    }
    gpio->value_fd = fd;
    gpio->exported = 1;

    return bb_gpio_set_direction(gpio, dir);
}

int bb_gpio_set_direction(bb_gpio_t *gpio, bb_gpio_direction_t dir) {
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/direction", gpio->num);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", dir == BB_GPIO_OUT ? "out" : "in");
    fclose(f);
    return 0;
}

int bb_gpio_read(bb_gpio_t *gpio) {
    if (gpio->value_fd < 0) return -1;
    char c;
    lseek(gpio->value_fd, 0, SEEK_SET);
    if (read(gpio->value_fd, &c, 1) != 1) return -1;
    return c == '1' ? 1 : 0;
}

int bb_gpio_write(bb_gpio_t *gpio, int value) {
    if (gpio->value_fd < 0) return -1;
    const char *v = value ? "1" : "0";
    return write(gpio->value_fd, v, 1) > 0 ? 0 : -1;
}

int bb_gpio_set_edge(bb_gpio_t *gpio, bb_gpio_edge_t edge) {
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO "/gpio%d/edge", gpio->num);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    const char *e[] = {"none", "rising", "falling", "both"};
    fprintf(f, "%s", e[edge]);
    fclose(f);
    return 0;
}

int bb_gpio_poll(bb_gpio_t *gpio, int timeout_ms) {
    if (gpio->value_fd < 0) return -1;
    struct pollfd pfd = { .fd = gpio->value_fd, .events = POLLPRI | POLLERR };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0) {
        // Consume the event by reading, then seek back
        char c;
        lseek(gpio->value_fd, 0, SEEK_SET);
        if (read(gpio->value_fd, &c, 1) < 0) return -1;
        return bb_gpio_read(gpio);  // Return current value after event
    }
    return ret; // 0 = timeout, -1 = error
}

void bb_gpio_close(bb_gpio_t *gpio) {
    if (gpio->value_fd >= 0) {
        close(gpio->value_fd);
        gpio->value_fd = -1;
    }
    if (gpio->exported) {
        FILE *f = fopen(SYSFS_GPIO "/unexport", "w");
        if (f) { fprintf(f, "%d", gpio->num); fclose(f); }
        gpio->exported = 0;
    }
}
