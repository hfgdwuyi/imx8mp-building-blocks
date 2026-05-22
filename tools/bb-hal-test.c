/*
 * bb-hal-test - Quick HAL validation tool
 * Tests I2C probe, SPI info, GPIO read
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libbb/bb_hal_i2c.h"
#include "../libbb/bb_hal_spi.h"
#include "../libbb/bb_hal_gpio.h"

int main(void) {
    int failures = 0;

    // --- I2C probe ---
    printf("=== I2C Bus Scan ===\n");
    const char *i2c_buses[] = {"/dev/i2c-0", "/dev/i2c-1", "/dev/i2c-2", "/dev/i2c-3", "/dev/i2c-4"};
    for (int b = 0; b < 5; b++) {
        bb_i2c_t i2c;
        if (bb_i2c_open(&i2c, i2c_buses[b]) < 0) {
            printf("%s: open failed\n", i2c_buses[b]);
            failures++;
            continue;
        }
        printf("%s: ", i2c_buses[b]);
        int found = 0;
        for (int addr = 0x03; addr <= 0x77; addr++) {
            if (bb_i2c_probe(&i2c, (uint8_t)addr) == 0) {
                printf("0x%02x ", addr);
                found++;
            }
        }
        if (!found) printf("(no devices)");
        printf("\n");
        bb_i2c_close(&i2c);
    }

    // --- SPI info ---
    printf("\n=== SPI Check ===\n");
    bb_spi_t spi;
    if (bb_spi_open(&spi, "/dev/spidev1.0", 1000000, 0, 8) == 0) {
        printf("/dev/spidev1.0: opened OK (fd=%d, speed=%u, mode=%d, bits=%d)\n",
               spi.fd, spi.speed_hz, spi.mode, spi.bits_per_word);
        bb_spi_close(&spi);
    } else {
        printf("/dev/spidev1.0: open FAILED\n");
        failures++;
    }

    // --- GPIO read (try a known pin if exported) ---
    printf("\n=== GPIO Check ===\n");
    int gpio_nums[] = {124};  // gpio124 was already exported
    for (size_t i = 0; i < sizeof(gpio_nums)/sizeof(gpio_nums[0]); i++) {
        bb_gpio_t gpio;
        if (bb_gpio_open(&gpio, gpio_nums[i], BB_GPIO_IN) == 0) {
            int val = bb_gpio_read(&gpio);
            printf("GPIO%d: value=%d\n", gpio_nums[i], val);
            bb_gpio_close(&gpio);
        } else {
            printf("GPIO%d: open FAILED\n", gpio_nums[i]);
            failures++;
        }
    }

    printf("\n=== Result: %d failures ===\n", failures);
    return failures;
}
