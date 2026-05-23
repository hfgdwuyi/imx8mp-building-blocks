/*
 * bb-hal-test - HAL validation tool for i.MX8MP
 *
 * Verifies:
 *   I2C:   PCF8563 RTC on i2c-0 (0x51) - read time registers
 *   SPI:   /dev/spidev1.0 interface
 *   GPIO:  gpio124 (headphone detect, input)
 *   LED:   led1, led2 via /sys/class/leds/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../libbb/bb_hal_i2c.h"
#include "../libbb/bb_hal_spi.h"
#include "../libbb/bb_hal_gpio.h"
#include "../libbb/bb_hal_led.h"

static int failures = 0;
#define PASS() printf("  PASS\n")
#define FAIL(msg) do { printf("  FAIL: %s\n", msg); failures++; } while(0)

/* ---- I2C: Scan all buses ---- */
static void test_i2c_scan(void) {
    printf("\n=== I2C Bus Scan ===\n");
    const char *buses[] = {"/dev/i2c-0", "/dev/i2c-1", "/dev/i2c-2", "/dev/i2c-3", "/dev/i2c-4"};
    const char *names[] = {"i2c-0 (PMIC)", "i2c-1 (Camera)", "i2c-2 (Periph)", "i2c-3 (Touch)", "i2c-4 (HDMI)"};

    for (int b = 0; b < 5; b++) {
        bb_i2c_t i2c;
        printf("%-22s: ", names[b]);
        if (bb_i2c_open(&i2c, buses[b]) < 0) {
            printf("open FAILED\n");
            failures++;
            continue;
        }
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
}

/* ---- I2C: PCF8563 RTC read (i2c-0, 0x51) ---- */
static void test_i2c_pcf8563(void) {
    printf("\n=== I2C: PCF8563 RTC (i2c-0, 0x51) ===\n");

    bb_i2c_t i2c;
    if (bb_i2c_open(&i2c, "/dev/i2c-0") < 0) {
        printf("  Cannot open /dev/i2c-0\n");
        FAIL("open i2c-0");
        return;
    }

    if (bb_i2c_probe(&i2c, 0x51) < 0) {
        FAIL("probe 0x51");
        bb_i2c_close(&i2c);
        return;
    }
    printf("  Device ACK'd at 0x51\n");

    // Read BCD time registers: addr 0x02, 7 bytes
    // VL(bit7)|sec, min, hour, day, weekday, month|century, year
    uint8_t reg = 0x02;
    uint8_t buf[7] = {0};
    if (bb_i2c_write_read(&i2c, 0x51, &reg, 1, buf, 7) == 0) {
        int vl = (buf[0] & 0x80) ? 1 : 0;
        int sec  = ((buf[0] >> 4) & 0x07) * 10 + (buf[0] & 0x0f);
        int min  = ((buf[1] >> 4) & 0x07) * 10 + (buf[1] & 0x0f);
        int hour = ((buf[2] >> 4) & 0x03) * 10 + (buf[2] & 0x0f);
        int day  = ((buf[3] >> 4) & 0x03) * 10 + (buf[3] & 0x0f);
        int mon  = ((buf[5] >> 4) & 0x01) * 10 + (buf[5] & 0x0f);
        int year = ((buf[6] >> 4) & 0x0f) * 10 + (buf[6] & 0x0f);

        printf("  PCF8563: 20%02d-%02d-%02d %02d:%02d:%02d (VL=%d)\n",
               year, mon, day, hour, min, sec, vl);
        if (vl) printf("  WARNING: Voltage low - RTC lost power!\n");
        PASS();
    } else {
        FAIL("write_read RTC registers (kernel driver may own this device)");
    }

    bb_i2c_close(&i2c);
}

/* ---- SPI: Interface test ---- */
static void test_spi(void) {
    printf("\n=== SPI: /dev/spidev1.0 ===\n");

    bb_spi_t spi;
    if (bb_spi_open(&spi, "/dev/spidev1.0", 1000000, BB_SPI_MODE_0, 8) < 0) {
        printf("  No SPI device available\n");
        FAIL("open spidev1.0");
        return;
    }
    printf("  Opened: speed=%u Hz, mode=%d, bits=%d\n",
           spi.speed_hz, spi.mode, spi.bits_per_word);

    uint8_t tx[4] = {0x9F, 0x00, 0x00, 0x00};
    uint8_t rx[4] = {0};
    if (bb_spi_transfer(&spi, tx, rx, 4) >= 0) {
        printf("  Transfer OK (rx: %02x %02x %02x %02x)\n", rx[0], rx[1], rx[2], rx[3]);
        PASS();
    } else {
        FAIL("transfer");
    }

    bb_spi_close(&spi);
}

/* ---- GPIO: Export, write, read, unexport via HAL ---- */
static void test_gpio(void) {
    printf("\n=== GPIO: HAL full cycle (gpio15) ===\n");

    bb_gpio_t gpio;

    // 1. Export + set direction
    if (bb_gpio_open(&gpio, 15, BB_GPIO_OUT) < 0) {
        printf("  export FAILED\n");
        FAIL("gpio15 open");
        return;
    }
    printf("  Exported gpio15 as output\n");

    // 2. Write HIGH, read back
    bb_gpio_write(&gpio, 1);
    int v1 = bb_gpio_read(&gpio);
    printf("  Write 1 -> read %d %s\n", v1, v1 == 1 ? "OK" : "MISMATCH");
    if (v1 != 1) FAIL("gpio15 write/read 1");

    // 3. Write LOW, read back
    bb_gpio_write(&gpio, 0);
    int v2 = bb_gpio_read(&gpio);
    printf("  Write 0 -> read %d %s\n", v2, v2 == 0 ? "OK" : "MISMATCH");
    if (v2 != 0) FAIL("gpio15 write/read 0");

    // 4. Unexport
    bb_gpio_close(&gpio);
    printf("  Unexported, HAL GPIO cycle complete\n");
    PASS();
}

/* ---- LED: User LEDs via sysfs ---- */
static void test_led(void) {
    printf("\n=== LED: User LEDs (led1, led2) ===\n");

    const char *names[] = {"led1", "led2"};
    for (int i = 0; i < 2; i++) {
        bb_led_t led;
        printf("  %s: ", names[i]);
        if (bb_led_open(&led, names[i]) < 0) {
            printf("open FAILED\n");
            FAIL(names[i]);
            continue;
        }

        printf("max_brightness=%d, toggling... ", led.max_brightness);
        bb_led_on(&led);
        usleep(200000);
        bb_led_off(&led);
        printf("OK\n");
        PASS();
    }
}

int main(void) {
    printf("=== bb-hal-test: i.MX8MP HAL Verification ===\n");
    printf("Board: Forlinx OK8MPlus-C\n");

    test_i2c_scan();
    test_i2c_pcf8563();
    test_spi();
    test_gpio();
    test_led();

    printf("\n=== Result: %d failures ===\n", failures);
    return failures;
}
