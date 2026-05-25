#ifndef BB_BOARD_H
#define BB_BOARD_H

/*
 * Board abstraction layer — all board-specific values in one place.
 *
 * Build: make cross CFLAGS="-DBOARD_FORLINX_OK8MPC"
 *        make cross CFLAGS="-DBOARD_NXP_IMX8MP_EVK"
 */

// ---------------------------------------------------------------------------
// Board selection — exactly one must be defined
// ---------------------------------------------------------------------------
#if !defined(BOARD_FORLINX_OK8MPC) && !defined(BOARD_NXP_IMX8MP_EVK)
#  warning "No board defined — defaulting to BOARD_NXP_IMX8MP_EVK"
#  define BOARD_NXP_IMX8MP_EVK
#endif

#if defined(BOARD_FORLINX_OK8MPC) && defined(BOARD_NXP_IMX8MP_EVK)
#  error "Multiple boards defined — pick exactly one"
#endif

// ---------------------------------------------------------------------------
// NXP i.MX8MP EVK (official evaluation kit)
// ---------------------------------------------------------------------------
#if defined(BOARD_NXP_IMX8MP_EVK)

#  define BB_PRODUCT_NAME      "NXP i.MX8MP EVK"
#  define BB_DTB_FILE          "imx8mp-evk.dtb"
#  define BB_CONSOLE_DEV       "/dev/ttymxc1"
#  define BB_CONSOLE_BAUD      115200

// LEDs — EVK has "yellow:status" user LED (gpio-80, active high)
#  define BB_LED_HEARTBEAT     "yellow:status"
#  define BB_LED1              "yellow:status"
#  define BB_LED2              ""              // Only one user LED on EVK

// SPI — EVK exposes ECSPI3 as spidev1.0 on J18 header
#  define BB_SPI_DEV           "/dev/spidev1.0"

// I2C buses — EVK has i2c-0 (PMIC), i2c-1, i2c-2, i2c-6 (no i2c-3/4/5)
#  define BB_I2C_COUNT         4
#  define BB_I2C_BUSES         {0, 1, 2, 6}
#  define BB_I2C_LABELS        {"i2c-0 (PMIC)","i2c-1","i2c-2","i2c-6"}

// GPIO — use an unused gpio pin (J18 pin 11 = GPIO1_IO13 = gpio 13)
// gpio-10 is PCA9539-reset, avoid. gpio-13 should be available.
#  define BB_GPIO_TEST_PIN     13

// UART — EVK has ttymxc1 (debug console) and ttymxc2. ttymxc0 is not available.
#  define BB_UART_COUNT        2
#  define BB_UART_DEVS         {"/dev/ttymxc1","/dev/ttymxc2"}
#  define BB_UART_LABELS       {"ttymxc1 (console)","ttymxc2"}

// PWM — EVK has pwmchip0, pwmchip1, pwmchip2
#  define BB_PWM_TEST_CHIPS    {0, 1, 2}
#  define BB_PWM_TEST_COUNT    3

// RTC
#  define BB_RTC_DEV           "/dev/rtc0"

// Watchdog
#  define BB_WDG_DEV           "/dev/watchdog0"

// Root device (SD card: mmcblk1, eMMC: mmcblk0)
#  define BB_ROOT_DEV          "/dev/mmcblk1"

// eMMC/SD partition layout (EVK: boot from SD card, single partition by default)
// For A/B update scheme, partition SD card with these numbers:
#  define BB_PART_BOOT_A       1
#  define BB_PART_BOOT_B       2
#  define BB_PART_ROOTFS_A     3
#  define BB_PART_ROOTFS_B     4
#  define BB_PART_RECOVERY     5
#  define BB_PART_PERSIST      6
#  define BB_PART_MFG          7
#  define BB_PART_LOG          8

// Network deploy target
#  define BB_DEPLOY_HOST       "192.168.1.100"
#  define BB_DEPLOY_USER       "root"
#  define BB_INSTALL_PREFIX    "/opt/building-blocks"

#endif // BOARD_NXP_IMX8MP_EVK

// ---------------------------------------------------------------------------
// Forlinx OK8MPlus-C
// ---------------------------------------------------------------------------
#if defined(BOARD_FORLINX_OK8MPC)

#  define BB_PRODUCT_NAME      "Forlinx OK8MPlus-C"
#  define BB_DTB_FILE          "imx8mp-ok8mplus-c.dtb"
#  define BB_CONSOLE_DEV       "/dev/ttymxc0"
#  define BB_CONSOLE_BAUD      115200

// LEDs
#  define BB_LED_HEARTBEAT     "heartbeat"
#  define BB_LED1              "led1"
#  define BB_LED2              "led2"

// SPI
#  define BB_SPI_DEV           "/dev/spidev1.0"

// I2C buses
#  define BB_I2C_COUNT         5
#  define BB_I2C_BUSES         {0, 1, 2, 3, 4}
#  define BB_I2C_LABELS        {"i2c-0 (PMIC)","i2c-1 (Camera)","i2c-2 (Periph)","i2c-3 (Touch)","i2c-4 (HDMI)"}

// GPIO
#  define BB_GPIO_TEST_PIN     15

// UART
#  define BB_UART_COUNT        3
#  define BB_UART_DEVS         {"/dev/ttymxc0","/dev/ttymxc1","/dev/ttymxc2"}
#  define BB_UART_LABELS       {"ttymxc0 (BT?)","ttymxc1 (console)","ttymxc2"}

// PWM
#  define BB_PWM_TEST_CHIPS    {0, 1}
#  define BB_PWM_TEST_COUNT    2

// RTC
#  define BB_RTC_DEV           "/dev/rtc0"

// Watchdog
#  define BB_WDG_DEV           "/dev/watchdog0"

// Root device
#  define BB_ROOT_DEV          "/dev/mmcblk0"

// Partition layout
#  define BB_PART_BOOT_A       4
#  define BB_PART_BOOT_B       5
#  define BB_PART_ROOTFS_A     6
#  define BB_PART_ROOTFS_B     7
#  define BB_PART_RECOVERY     8
#  define BB_PART_PERSIST      9
#  define BB_PART_MFG          10
#  define BB_PART_LOG          11

// Network deploy target
#  define BB_DEPLOY_HOST       "192.168.0.232"
#  define BB_DEPLOY_USER       "root"
#  define BB_INSTALL_PREFIX    "/opt/building-blocks"

#endif // BOARD_FORLINX_OK8MPC

#endif // BB_BOARD_H
