#!/bin/sh
# Building Blocks - On-Board Test Suite
# Runs on i.MX8MP EVK

BB_DIR="/tmp/bb-test/build/bin"
PASS=0
FAIL=0

check() {
    local name="$1"
    local expected="$2"
    local actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (expected: $expected, got: $actual)"
        FAIL=$((FAIL + 1))
    fi
}

echo "=========================================="
echo " Building Blocks On-Board Test Suite"
echo " Board: $(cat /proc/device-tree/model 2>/dev/null || echo unknown)"
echo " Kernel: $(uname -r)"
echo "=========================================="

# ---- TEST 1: HAL Full Test ----
echo ""
echo "=== TEST 1: HAL Test (8 modules) ==="
$BB_DIR/bb-hal-test 2>&1
RET=$?
check "HAL test overall" "0" "$RET"

# ---- TEST 2: GPIO gpiochip v2 path ----
echo ""
echo "=== TEST 2: GPIO gpiochip v2 verification ==="
GPIOCHIP_COUNT=$(ls /dev/gpiochip* 2>/dev/null | wc -l)
echo "  gpiochip devices: $GPIOCHIP_COUNT"
check "gpiochip devices present" "0" "$(test $GPIOCHIP_COUNT -gt 0 && echo 0 || echo 1)"

SYSFS_EXISTS=$(test -d /sys/class/gpio && echo 1 || echo 0)
echo "  sysfs GPIO exists: $SYSFS_EXISTS"
check "gpiochip v2 used (no sysfs)" "0" "$SYSFS_EXISTS"

# ---- TEST 3: SPI ----
echo ""
echo "=== TEST 3: SPI device ==="
SPIDEV=$(ls /dev/spidev* 2>/dev/null | head -1)
echo "  Device: $SPIDEV"
check "SPI device exists" "0" "$(test -n "$SPIDEV" && echo 0 || echo 1)"

# ---- TEST 4: I2C buses ----
echo ""
echo "=== TEST 4: I2C buses ==="
I2C_COUNT=$(ls /dev/i2c-* 2>/dev/null | wc -l)
echo "  I2C buses: $I2C_COUNT"
check "I2C buses present" "0" "$(test $I2C_COUNT -gt 0 && echo 0 || echo 1)"

# ---- TEST 5: PWM ----
echo ""
echo "=== TEST 5: PWM ==="
PWM_COUNT=$(ls /sys/class/pwm/pwmchip* 2>/dev/null | wc -l)
echo "  PWM chips: $PWM_COUNT"
check "PWM chips present" "0" "$(test $PWM_COUNT -gt 0 && echo 0 || echo 1)"

# ---- TEST 6: RTC ----
echo ""
echo "=== TEST 6: RTC ==="
RTC_DEV=$(ls /dev/rtc* 2>/dev/null | head -1)
echo "  Device: $RTC_DEV"
check "RTC device exists" "0" "$(test -n "$RTC_DEV" && echo 0 || echo 1)"

# ---- TEST 7: Watchdog ----
echo ""
echo "=== TEST 7: Watchdog ==="
WDG_DEV=$(ls /dev/watchdog* 2>/dev/null | head -1)
echo "  Device: $WDG_DEV"
check "Watchdog device exists" "0" "$(test -n "$WDG_DEV" && echo 0 || echo 1)"

# ---- TEST 8: UART ----
echo ""
echo "=== TEST 8: UART ==="
UART_COUNT=$(ls /dev/ttymxc* 2>/dev/null | wc -l)
echo "  UART devices: $UART_COUNT"
check "UART devices present" "0" "$(test $UART_COUNT -gt 0 && echo 0 || echo 1)"

# ---- TEST 9: bb-update ----
echo ""
echo "=== TEST 9: bb-update ==="
SLOT=$($BB_DIR/bb-update slot 2>&1)
echo "  Slot: $SLOT"
check "bb-update slot returns" "0" "$(test -n "$SLOT" && echo 0 || echo 1)"

$BB_DIR/bb-update create --from 1.0 --to 2.0 --product test --rootfs /dev/null /tmp/test.bbu 2>&1
RET=$?
check "bb-update create" "0" "$RET"

$BB_DIR/bb-update verify /tmp/test.bbu 2>&1
RET=$?
check "bb-update verify" "0" "$RET"

BBU_SIZE=$(stat -c%s /tmp/test.bbu 2>/dev/null)
echo "  .bbu size: $BBU_SIZE bytes"
check ".bbu file created" "0" "$(test "$BBU_SIZE" -gt 500 && echo 0 || echo 1)"

# ---- TEST 10: bb-busd + bb-cli ----
echo ""
echo "=== TEST 10: bb-busd + bb-cli ==="
rm -f /run/bb-bus.sock
$BB_DIR/bb-busd &
sleep 1
BUSD_PID=$!

if [ -S /run/bb-bus.sock ]; then
    echo "  busd socket: OK"
    PONG=$($BB_DIR/bb-cli ping 2>&1)
    echo "  PING response: $PONG"
    check "bb-cli ping/pong" "Bus: PONG" "$PONG"
else
    echo "  FAIL: busd socket not created"
    FAIL=$((FAIL + 1))
fi

kill $BUSD_PID 2>/dev/null
wait $BUSD_PID 2>/dev/null

# ---- TEST 11: bb-led ----
echo ""
echo "=== TEST 11: bb-led ==="
rm -f /run/bb-bus.sock
$BB_DIR/bb-busd &
sleep 1

LED_OUT=$($BB_DIR/bb-led 2>&1 & sleep 2; kill %1 2>/dev/null)
echo "  LED output: $LED_OUT"
check "bb-led opens LED" "0" "$(echo "$LED_OUT" | grep -q "opened" && echo 0 || echo 1)"

kill %1 2>/dev/null
wait 2>/dev/null

# ---- TEST 12: bb_update (no crypto mode) ----
echo ""
echo "=== TEST 12: bb_update manifest integrity ==="
MANIFEST_OK=$($BB_DIR/bb-update verify /tmp/test.bbu 2>&1 | grep -c "Signature: OK")
check "bb-update manifest signature field" "1" "$MANIFEST_OK"

# ---- Summary ----
echo ""
echo "=========================================="
echo " RESULTS: $PASS pass, $FAIL fail"
echo "=========================================="
rm -f /tmp/test.bbu /run/bb-bus.sock
exit $FAIL
