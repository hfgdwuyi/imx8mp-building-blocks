/*
 * bb-hal-test - HAL validation tool for i.MX8MP (all 8 modules)
 *
 * Tests: I2C, SPI, GPIO, LED, PWM, RTC, Watchdog, UART
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
#include "../libbb/bb_hal_pwm.h"
#include "../libbb/bb_hal_rtc.h"
#include "../libbb/bb_hal_wdg.h"
#include "../libbb/bb_hal_uart.h"

static int failures = 0;
#define PASS() printf("  PASS\n")
#define FAIL(msg) do { printf("  FAIL: %s\n", msg); failures++; } while(0)

/* ---- I2C: Scan 5 buses ---- */
static void test_i2c_scan(void) {
    printf("\n=== I2C Bus Scan ===\n");
    const char *buses[] = {"/dev/i2c-0", "/dev/i2c-1", "/dev/i2c-2", "/dev/i2c-3", "/dev/i2c-4"};
    const char *names[] = {"i2c-0 (PMIC)","i2c-1 (Camera)","i2c-2 (Periph)","i2c-3 (Touch)","i2c-4 (HDMI)"};

    for (int b = 0; b < 5; b++) {
        bb_i2c_t i2c;
        printf("%-22s: ", names[b]);
        if (bb_i2c_open(&i2c, buses[b]) < 0) {
            printf("open FAILED\n"); failures++; continue;
        }
        int found = 0;
        for (int addr = 0x03; addr <= 0x77; addr++) {
            if (bb_i2c_probe(&i2c, (uint8_t)addr) == 0) { printf("0x%02x ", addr); found++; }
        }
        if (!found) printf("(no devices)");
        printf("\n");
        bb_i2c_close(&i2c);
    }
}

/* ---- I2C: PCF8563 RTC read ---- */
static void test_i2c_pcf8563(void) {
    printf("\n=== I2C: PCF8563 RTC (i2c-0, 0x51) ===\n");
    bb_i2c_t i2c;
    if (bb_i2c_open(&i2c, "/dev/i2c-0") < 0) { FAIL("open i2c-0"); return; }
    if (bb_i2c_probe(&i2c, 0x51) < 0)       { FAIL("probe 0x51"); bb_i2c_close(&i2c); return; }

    uint8_t reg = 0x02, buf[7] = {0};
    if (bb_i2c_write_read(&i2c, 0x51, &reg, 1, buf, 7) == 0) {
        int sec  = ((buf[0]>>4)&0x07)*10 + (buf[0]&0x0f);
        int min  = ((buf[1]>>4)&0x07)*10 + (buf[1]&0x0f);
        int hour = ((buf[2]>>4)&0x03)*10 + (buf[2]&0x0f);
        int day  = ((buf[3]>>4)&0x03)*10 + (buf[3]&0x0f);
        int mon  = ((buf[5]>>4)&0x01)*10 + (buf[5]&0x0f);
        int year = ((buf[6]>>4)&0x0f)*10 + (buf[6]&0x0f);
        printf("  RTC: 20%02d-%02d-%02d %02d:%02d:%02d (VL=%d)\n",
               year, mon, day, hour, min, sec, (buf[0]&0x80)?1:0);
        PASS();
    } else { FAIL("read RTC registers"); }
    bb_i2c_close(&i2c);
}

/* ---- SPI: Interface test ---- */
static void test_spi(void) {
    printf("\n=== SPI: /dev/spidev1.0 ===\n");
    bb_spi_t spi;
    if (bb_spi_open(&spi, "/dev/spidev1.0", 1000000, BB_SPI_MODE_0, 8) < 0) {
        FAIL("open spidev1.0"); return;
    }
    printf("  Opened: speed=%u mode=%d bits=%d\n", spi.speed_hz, spi.mode, spi.bits_per_word);

    uint8_t tx[4] = {0x9F,0x00,0x00,0x00}, rx[4] = {0};
    if (bb_spi_transfer(&spi, tx, rx, 4) >= 0) {
        printf("  Transfer OK (rx: %02x %02x %02x %02x)\n", rx[0],rx[1],rx[2],rx[3]);
        PASS();
    } else { FAIL("transfer"); }
    bb_spi_close(&spi);
}

/* ---- GPIO: Full cycle on gpio15 ---- */
static void test_gpio(void) {
    printf("\n=== GPIO: Full cycle (gpio15) ===\n");
    bb_gpio_t gpio;
    if (bb_gpio_open(&gpio, 15, BB_GPIO_OUT) < 0) { FAIL("gpio15 open"); return; }
    printf("  Exported gpio15 as output\n");

    bb_gpio_write(&gpio, 1);
    int v1 = bb_gpio_read(&gpio);
    printf("  Write 1 -> read %d ", v1);
    if (v1 != 1) { printf("MISMATCH\n"); FAIL("gpio15 write/read 1"); }
    else printf("OK\n");

    bb_gpio_write(&gpio, 0);
    int v2 = bb_gpio_read(&gpio);
    printf("  Write 0 -> read %d ", v2);
    if (v2 != 0) { printf("MISMATCH\n"); FAIL("gpio15 write/read 0"); }
    else printf("OK\n");

    bb_gpio_close(&gpio);
    PASS();
}

/* ---- LED: Toggle led1, led2 ---- */
static void test_led(void) {
    printf("\n=== LED: User LEDs (led1, led2) ===\n");
    const char *names[] = {"led1", "led2"};
    for (int i = 0; i < 2; i++) {
        bb_led_t led;
        printf("  %s: ", names[i]);
        if (bb_led_open(&led, names[i]) < 0) { printf("open FAILED\n"); FAIL(names[i]); continue; }
        printf("max=%d, toggling... ", led.max_brightness);
        bb_led_on(&led); usleep(150000); bb_led_off(&led);
        printf("OK\n");
        PASS();
    }
}

/* ---- PWM: Try pwmchip0 then pwmchip1 ---- */
static void test_pwm(void) {
    printf("\n=== PWM: pwmchip0/pwmchip1 ===\n");
    int tested = 0;
    for (int chip = 0; chip <= 1; chip++) {
        bb_pwm_t pwm;
        printf("  pwmchip%d/pwm0: ", chip);
        if (bb_pwm_open(&pwm, chip, 0) < 0) {
            printf("export BUSY (kernel-owned)\n");
            continue;
        }
        printf("exported, ");
        int ok = 1;
        if (bb_pwm_set_period(&pwm, 1000000) < 0)  { printf("period FAILED "); ok = 0; }
        if (bb_pwm_set_duty(&pwm, 500000) < 0)     { printf("duty FAILED "); ok = 0; }
        if (bb_pwm_enable(&pwm) < 0)                { printf("enable FAILED "); ok = 0; }
        if (ok) {
            printf("Period=1ms Duty=50%% Enabled OK\n");
            usleep(100000);
            bb_pwm_disable(&pwm);
            tested = 1;
        } else {
            printf("(not routed to pin)\n");
        }
        bb_pwm_close(&pwm);
    }
    if (tested) PASS(); else FAIL("All PWM channels busy (kernel-owned)");
}

/* ---- RTC: Read via /dev/rtc0 ioctl ---- */
static void test_rtc(void) {
    printf("\n=== RTC: /dev/rtc0 ===\n");
    bb_rtc_t rtc;
    if (bb_rtc_open(&rtc, "/dev/rtc0") < 0) { FAIL("open /dev/rtc0"); return; }

    bb_rtc_time_t t;
    if (bb_rtc_read(&rtc, &t) == 0) {
        printf("  RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
               t.year, t.mon, t.day, t.hour, t.min, t.sec);
        PASS();
    } else { FAIL("read RTC"); }

    bb_rtc_close(&rtc);
}

/* ---- Watchdog: Open, get timeout, kick ---- */
static void test_wdg(void) {
    printf("\n=== Watchdog: /dev/watchdog0 ===\n");
    bb_wdg_t wdg;
    if (bb_wdg_open(&wdg, "/dev/watchdog0") < 0) { FAIL("open /dev/watchdog0"); return; }

    int timeout = bb_wdg_get_timeout(&wdg);
    printf("  Current timeout: %d sec\n", timeout);

    // Only kick if timeout > 0 (safety: don't kick a disabled wdg)
    if (timeout > 0) {
        if (bb_wdg_kick(&wdg) == 0) {
            printf("  Kick OK\n");
            PASS();
        } else { FAIL("kick"); }
    } else {
        printf("  Timeout is 0, skipping kick (disabled)\n");
        PASS();
    }

    bb_wdg_close(&wdg);
}

/* ---- UART: Open, configure, TX on first available port ---- */
static void test_uart(void) {
    printf("\n=== UART: Serial ports ===\n");
    const char *ports[] = {"/dev/ttymxc0", "/dev/ttymxc1", "/dev/ttymxc2"};
    const char *notes[] = {"(BT?)", "(console)", ""};
    int tested = 0;

    for (int i = 0; i < 3; i++) {
        bb_uart_t uart;
        printf("  %s %s: ", ports[i], notes[i]);
        if (bb_uart_open(&uart, ports[i], BB_UART_BAUD_115200) < 0) {
            printf("open FAILED\n");
            FAIL(ports[i]);
            continue;
        }
        printf("opened OK, ");

        // Write test - will fail if port has HW flow control with unasserted CTS
        const char *msg = "AT\r\n";
        int w_ret = bb_uart_write(&uart, (const uint8_t *)msg, strlen(msg));
        if (w_ret == 0) {
            printf("TX OK\n");
            tested = 1;
        } else {
            printf("TX blocked (HW flow control?)\n");
        }
        bb_uart_close(&uart);
    }
    if (tested) PASS(); else FAIL("No writable UART (all blocked or in use)");
}

int main(void) {
    printf("=== bb-hal-test: i.MX8MP HAL Verification (8 modules) ===\n");
    printf("Board: Forlinx OK8MPlus-C\n");

    test_i2c_scan();
    test_i2c_pcf8563();
    test_spi();
    test_gpio();
    test_led();
    test_pwm();
    test_rtc();
    test_wdg();
    test_uart();

    printf("\n=== Result: %d failures ===\n", failures);
    return failures;
}
