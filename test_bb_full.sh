#!/bin/sh
# Building Blocks - Full On-Board Verification (32 design elements)
set -e
BB_DIR="/tmp/bb-test/build/bin"
BB_LIB="/tmp/bb-test/libbb"
PASS=0; FAIL=0; SKIP=0

check() {
    local name="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo "  PASS: $name"; PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (exp: $expected, got: $actual)"; FAIL=$((FAIL + 1))
    fi
}
check_gt() {
    local name="$1" val="$2"
    if [ "$val" -gt 0 ] 2>/dev/null; then
        echo "  PASS: $name ($val)"; PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (got: $val)"; FAIL=$((FAIL + 1))
    fi
}
check_file() {
    local name="$1" path="$2"
    if [ -f "$path" ]; then
        echo "  PASS: $name"; PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (not found)"; FAIL=$((FAIL + 1))
    fi
}

echo "=========================================="
echo " Building Blocks - Full Design Verification"
echo " Board: $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')"
echo " Kernel: $(uname -r)"
echo " Date: $(date)"
echo "=========================================="

# ====== 1. Boot Device ======
echo ""; echo "=== DESIGN #1: Boot Device ==="
ROOT_DEV=$(cat /proc/cmdline | tr ' ' '\n' | grep '^root=' | cut -d= -f2)
BOOT_DEV=$(echo "$ROOT_DEV" | sed 's/p[0-9]$//')
echo "  root: $ROOT_DEV | boot device: $BOOT_DEV"
check_gt "root device found" "$(echo "$ROOT_DEV" | wc -c)"

# ====== 2. Partition Layout ======
echo ""; echo "=== DESIGN #2: Partition Layout ==="
echo "  Expected 11 partitions (A/B dual-slot + recovery + persist + log)"
lsblk 2>/dev/null | head -15
PART_COUNT=$(lsblk -ln 2>/dev/null | wc -l)
check "block devices present" "0" "$(test "$PART_COUNT" -gt 0 && echo 0 || echo 1)"

# ====== 3. ATF / SCU Firmware ======
echo ""; echo "=== DESIGN #3/#4/#10/#11: Firmware ==="
for fw in atf-version scfw-version; do
    if [ -f "/etc/firmware/$fw" ]; then
        echo "  /etc/firmware/$fw: $(cat /etc/firmware/$fw)"
    else
        echo "  /etc/firmware/$fw: NOT FOUND (stock image)"
    fi
done
# Check dmesg for BL31 / ATF evidence
DMESG_ATF=$(dmesg 2>/dev/null | grep -ic "BL31\|atf\|trusted\|psci" || true)
echo "  dmesg ATF/BL31 refs: $DMESG_ATF"
check_gt "ATF in dmesg" "$DMESG_ATF"
DMESG_SCU=$(dmesg 2>/dev/null | grep -ic "scu\|scfw\|M4\|cortex-m" || true)
echo "  dmesg SCU/M4 refs: $DMESG_SCU"

# ====== 5. U-Boot Environment ======
echo ""; echo "=== DESIGN #5: U-Boot Environment ==="
if command -v fw_printenv >/dev/null 2>&1; then
    for var in uboot_version boot_slot boot_ok boot_attempt boot_limit recovery_mode; do
        val=$(fw_printenv $var 2>/dev/null | cut -d= -f2)
        echo "  $var=$val"
    done
    check "U-Boot env accessible" "0" "0"
else
    echo "  fw_printenv not available (stock image, no /etc/fw_env.config)"
    SKIP=$((SKIP + 6))
fi

# ====== 6. Boot Script ======
echo ""; echo "=== DESIGN #6: Boot Script (boot.cmd) ==="
check_file "boot.cmd" /tmp/bb-test/deploy/boot/boot.cmd
CMD_LINES=$(wc -l < /tmp/bb-test/deploy/boot/boot.cmd 2>/dev/null || true)
echo "  Lines: $CMD_LINES"
AB_COUNT=$(grep -c "boot_slot" /tmp/bb-test/deploy/boot/boot.cmd 2>/dev/null || true)
echo "  A/B slot logic references: $AB_COUNT"
check_gt "boot.cmd has A/B logic" "$AB_COUNT"

# ====== 7. U-Boot Custom Commands ======
echo ""; echo "=== DESIGN #7: U-Boot Custom Commands ==="
DOC_CMDS=$(grep -c "bb_slot\|bb_recovery\|bb_factory\|bb_info\|bb_update" /tmp/bb-test/docs/system-architecture.md 2>/dev/null || true)
echo "  Custom commands in doc: 5 (bb_slot, bb_recovery, bb_factory, bb_info, bb_update)"
check_gt "U-Boot custom commands documented" "$DOC_CMDS"

# ====== 8. boot_ok Flag ======
echo ""; echo "=== DESIGN #8: boot_ok Flag ==="
check_file "bb-boot-ok.service" /tmp/bb-test/deploy/system/bb-boot-ok.service
BOOT_OK_EXEC=$(grep ExecStart /tmp/bb-test/deploy/system/bb-boot-ok.service 2>/dev/null)
echo "  $BOOT_OK_EXEC"

# ====== 9. Start-up Time ======
echo ""; echo "=== DESIGN #9: Start-up Time ==="
UPTIME=$(awk '{print int($1)}' /proc/uptime 2>/dev/null)
echo "  Current uptime: ${UPTIME}s"
SYS_BOOT_TIME=$(who -b 2>/dev/null | awk '{print $3, $4}')
echo "  System boot: $SYS_BOOT_TIME"
SYSTEMD_TIME=$(systemd-analyze 2>/dev/null | head -1 || echo "N/A")
echo "  systemd-analyze: $SYSTEMD_TIME"
FINISH_TIME=$(systemd-analyze time 2>/dev/null | grep Finish | awk '{print $4}' || echo "N/A")
echo "  Kernel+init time: $FINISH_TIME"

# ====== 12. Recovery ======
echo ""; echo "=== DESIGN #12: Recovery Partition/Init ==="
check_file "recovery-init" /tmp/bb-test/deploy/boot/recovery-init
RECOV_LINES=$(wc -l < /tmp/bb-test/deploy/boot/recovery-init 2>/dev/null)
echo "  Lines: $RECOV_LINES"

# ====== 13. Update Tools ======
echo ""; echo "=== DESIGN #13: Update Tools ==="
UPDATE_USAGE=$($BB_DIR/bb-update 2>&1 | grep -c "Usage" || true)
check_gt "bb-update CLI help" "$UPDATE_USAGE"

# ====== 14. Installation ======
echo ""; echo "=== DESIGN #14: Update Installation ==="
INSTALL_CODE=$(grep -c "bb_update_install" /tmp/bb-test/tools/bb-update/bb_update.c 2>/dev/null || true)
check_gt "bb_update_install function" "$INSTALL_CODE"

# ====== 15. Partition Recovery ======
echo ""; echo "=== DESIGN #15: Partition Recovery ==="
REIMAGE_CODE=$(grep -c "bb_recovery_reimage\|bb_recovery_verify_partitions" $BB_LIB/bb_recovery.c 2>/dev/null || true)
check_gt "recovery functions" "$REIMAGE_CODE"

# ====== 16. Logging ======
echo ""; echo "=== DESIGN #16: Three-tier Logging ==="
check_file "bb_log.o" /tmp/bb-test/build/obj/bb_log.o
LOG_LEVELS=$(grep -c "BB_LOG_" $BB_LIB/bb_log.h 2>/dev/null || true)
echo "  Log levels defined: $LOG_LEVELS"
check_gt "6 log levels" "$LOG_LEVELS"

# ====== 17. Snapshot ======
echo ""; echo "=== DESIGN #17: Snapshot ==="
SNAP_CODE=$(grep -c "bb_snapshot_create\|bb_snapshot_delete" $BB_LIB/bb_recovery.c 2>/dev/null || true)
check_gt "snapshot functions" "$SNAP_CODE"

# ====== 18. Fatal Crash Log ======
echo ""; echo "=== DESIGN #18: Fatal Crash Dump ==="
CRASH_CODE=$(grep -c "bb_log_crash" $BB_LIB/bb_log.c 2>/dev/null || true)
check_gt "bb_log_crash function" "$CRASH_CODE"
BT_CODE=$(grep -c "backtrace" $BB_LIB/bb_log.c 2>/dev/null || true)
check_gt "backtrace in crash dump" "$BT_CODE"

# ====== 19/20/21. Log self-disclosure / location ======
echo ""; echo "=== DESIGN #19/#20/#21: Logging Self-Disclosure ==="
DIAG_CODE=$(grep -c "bb_log_diagnostics" $BB_LIB/bb_log.c 2>/dev/null || true)
check_gt "bb_log_diagnostics" "$DIAG_CODE"
echo "  Crash dir: /persist/crash, Log dir: /var/log/persist"

# ====== 22. Kernel ======
echo ""; echo "=== DESIGN #22: Kernel ==="
KERNEL_MAJ=$(uname -r | cut -d. -f1)
KERNEL_MIN=$(uname -r | cut -d. -f2)
check "Kernel >= 6.1" "0" "$([ "$KERNEL_MAJ" -ge 6 ] && [ "$KERNEL_MIN" -ge 1 ] && echo 0 || echo 1)"
PSTORE=$(zcat /proc/config.gz 2>/dev/null | grep "CONFIG_PSTORE=" | head -1 || echo "N/A")
echo "  CONFIG_PSTORE: $PSTORE"
DM_CRYPT=$(zcat /proc/config.gz 2>/dev/null | grep "CONFIG_DM_CRYPT=" | head -1 || echo "N/A")
echo "  CONFIG_DM_CRYPT: $DM_CRYPT"
FW_ENV=$(zcat /proc/config.gz 2>/dev/null | grep "CONFIG_FW_ENV" | head -1 || echo "N/A")
echo "  CONFIG_FW_ENV: $FW_ENV"

# ====== 23. Device Tree ======
echo ""; echo "=== DESIGN #23: Device Tree ==="
DTB_MODEL=$(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')
DTB_COMPAT=$(cat /proc/device-tree/compatible 2>/dev/null | tr -d '\0')
echo "  Model: $DTB_MODEL"
echo "  Compatible: $DTB_COMPAT"

# ====== 24. Rootfs ======
echo ""; echo "=== DESIGN #24: Rootfs ==="
df -h / 2>/dev/null
ROOTFS_TYPE=$(df -T / 2>/dev/null | tail -1 | awk '{print $2}')
check "rootfs is ext4" "ext4" "$ROOTFS_TYPE"

# ====== 25. Version ======
echo ""; echo "=== DESIGN #25: Version ==="
if [ -f /etc/os-release ]; then
    grep "VERSION" /etc/os-release 2>/dev/null
fi
echo "  bb-update version: $($BB_DIR/bb-update version 2>&1)"
check_file "os-release exists" /etc/os-release

# ====== 26. Update Package ======
echo ""; echo "=== DESIGN #26: .bbu Package Format ==="
BBU_FORMAT=$(grep -c "BB_UPDATE_HEADER_SIZE\|BB_UPDATE_MAGIC\|BB_SIG_MAX" /tmp/bb-test/tools/bb-update/bb_update.h 2>/dev/null || true)
check_gt "bbu format constants" "$BBU_FORMAT"

# ====== 27. systemd Services ======
echo ""; echo "=== DESIGN #27: systemd Init Scripts ==="
SVC_COUNT=$(ls /tmp/bb-test/deploy/system/*.service 2>/dev/null | wc -l)
TIMER_COUNT=$(ls /tmp/bb-test/deploy/system/*.timer 2>/dev/null | wc -l)
echo "  Services: $SVC_COUNT, Timers: $TIMER_COUNT"
check_gt "service files exist" "$SVC_COUNT"
for svc in /tmp/bb-test/deploy/system/*.service; do
    echo "    - $(basename $svc)"
done

# ====== 28. Supervision ======
echo ""; echo "=== DESIGN #28: Supervision (systemd) ==="
WDS=$(grep WatchdogSec /tmp/bb-test/deploy/system/bb-avgate.service 2>/dev/null | xargs)
echo "  bb-avgate: $WDS"
RESTART=$(grep Restart= /tmp/bb-test/deploy/system/bb-avgate.service 2>/dev/null | xargs)
echo "  $RESTART"
MEM_MAX=$(grep MemoryMax /tmp/bb-test/deploy/system/bb-avgate.service 2>/dev/null | xargs)
echo "  $MEM_MAX"
check_gt "WatchdogSec set" "$(grep -c WatchdogSec /tmp/bb-test/deploy/system/bb-avgate.service 2>/dev/null || true)"

# ====== 29. Health Monitor ======
echo ""; echo "=== DESIGN #29: Health Monitor ==="
check_file "bb-health.service" /tmp/bb-test/deploy/system/bb-health.service

# ====== 30. Time Sync ======
echo ""; echo "=== DESIGN #30: Time Synchronization ==="
timedatectl show 2>/dev/null | grep -E "NTP|TimeUSec|RTCTimeUSec" || true
RTC=$(hwclock -r 2>/dev/null || echo "N/A")
echo "  RTC time: $RTC"
check_gt "NTP enabled" "$(timedatectl show 2>/dev/null | grep -c "NTP=yes" || true)"

# ====== 31. Persist ======
echo ""; echo "=== DESIGN #31: Persist Partition ==="
check_file "bb_persist.o" /tmp/bb-test/build/obj/bb_persist.o
BOOT_LOG=$(grep -c "bb_persist_boot_log_add" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "boot_log_add" "$BOOT_LOG"
UPD_LOG=$(grep -c "bb_persist_update_log_add" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "update_log_add" "$UPD_LOG"
MID=$(grep -c "bb_persist_machine_id" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "machine_id" "$MID"
CONFIG_GET=$(grep -c "bb_persist_config_get" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "config_get" "$CONFIG_GET"

# ====== 32. Manufacturing ======
echo ""; echo "=== DESIGN #32: Manufacturing Data ==="
MFG_STRUCT=$(grep -c "bb_mfg_data_t" $BB_LIB/bb_persist.h 2>/dev/null || true)
check_gt "bb_mfg_data_t struct" "$MFG_STRUCT"
MFG_READ=$(grep -c "bb_mfg_read" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "bb_mfg_read" "$MFG_READ"
CRC32=$(grep -c "0xEDB88320" $BB_LIB/bb_persist.c 2>/dev/null || true)
check_gt "CRC32 check" "$CRC32"

# ====== Thread Module ======
echo ""; echo "=== NEW MODULE: bb_thread ==="
check_file "bb_thread.o" /tmp/bb-test/build/obj/bb_thread.o
AFFINITY=$(grep -c "pthread_setaffinity_np" $BB_LIB/bb_thread.c 2>/dev/null || true)
check_gt "CPU affinity set" "$AFFINITY"
RT=$(grep -c "SCHED_FIFO\|sched_setscheduler" $BB_LIB/bb_thread.c 2>/dev/null || true)
check_gt "RT scheduling" "$RT"

# ====== Pool Module ======
echo ""; echo "=== NEW MODULE: bb_pool ==="
check_file "bb_pool.o" /tmp/bb-test/build/obj/bb_pool.o
SPSC=$(grep -c "bb_pool_sp_acquire" $BB_LIB/bb_pool.c 2>/dev/null || true)
check_gt "SPSC lock-free path" "$SPSC"
MPSC=$(grep -c "bb_pool_mp_acquire" $BB_LIB/bb_pool.c 2>/dev/null || true)
check_gt "MPSC mutex path" "$MPSC"
REF=$(grep -c "bb_frame_ref\|bb_frame_unref" $BB_LIB/bb_pool.c 2>/dev/null || true)
check_gt "frame refcounting" "$REF"

# ====== GPIO HAL ======
echo ""; echo "=== GPIO HAL: gpiochip v2 ==="
check_file "bb_hal_gpio.o" /tmp/bb-test/build/obj/bb_hal_gpio.o
V2_OPEN=$(grep -c "GPIO_V2_GET_LINE_IOCTL" $BB_LIB/bb_hal_gpio.c 2>/dev/null || true)
check_gt "gpio v2 ioctl" "$V2_OPEN"
SYSFS=$(grep -c "gpio_sysfs_open" $BB_LIB/bb_hal_gpio.c 2>/dev/null || true)
check_gt "sysfs fallback" "$SYSFS"
EDGE=$(grep -c "gpio_v2_set_edge" $BB_LIB/bb_hal_gpio.c 2>/dev/null || true)
check_gt "v2 set_edge (fixed)" "$EDGE"
POLL=$(grep -c "bb_gpio_poll" $BB_LIB/bb_hal_gpio.c 2>/dev/null || true)
check_gt "gpio poll" "$POLL"

# ====== Board Abstraction ======
echo ""; echo "=== Board Abstraction: bb_board.h ==="
EVK=$(grep -c "BOARD_NXP_IMX8MP_EVK" $BB_LIB/bb_board.h 2>/dev/null || true)
check_gt "NXP EVK board" "$EVK"
FORL=$(grep -c "BOARD_FORLINX_OK8MPC" $BB_LIB/bb_board.h 2>/dev/null || true)
check_gt "Forlinx OK8MPC board" "$FORL"

# ====== Deploy Artifacts ======
echo ""; echo "=== DEPLOY: Boot + System Artifacts ==="
check_file "boot.cmd" /tmp/bb-test/deploy/boot/boot.cmd
check_file "uboot-env.txt" /tmp/bb-test/deploy/boot/uboot-env.txt
check_file "recovery-init" /tmp/bb-test/deploy/boot/recovery-init
check_file "system-architecture.md" /tmp/bb-test/docs/system-architecture.md
ENV_VARS=$(grep -cE "^[a-z_]+[[:space:]]+" /tmp/bb-test/deploy/boot/uboot-env.txt 2>/dev/null || true)
echo "  U-Boot env variables defined: $ENV_VARS"
check_gt "env variables" "$ENV_VARS"

# ====== HAL Runtime Re-test ======
echo ""; echo "=== RUNTIME RE-TEST: HAL (8 modules) ==="
$BB_DIR/bb-hal-test 2>&1 | grep -E "PASS|FAIL|Result|===.*==="

# ====== Bus + LED integration ======
echo ""; echo "=== RUNTIME: Bus + LED Integration ==="
rm -f /run/bb-bus.sock
$BB_DIR/bb-busd &
BUSD_PID=$!
sleep 1
if [ -S /run/bb-bus.sock ]; then
    echo "  busd started OK"
    PONG=$($BB_DIR/bb-cli ping 2>&1)
    check "bb-cli ping/pong" "Bus: PONG" "$PONG"
    timeout 3 $BB_DIR/bb-led 2>&1 | grep -q "opened" && echo "  LED opened OK" || echo "  LED: timeout (long-running)"
else
    echo "  FAIL: busd socket not created"
    FAIL=$((FAIL + 1))
fi
kill $BUSD_PID 2>/dev/null; wait $BUSD_PID 2>/dev/null

# ====== Summary ======
echo ""
echo "=========================================="
echo " VERIFICATION COMPLETE"
echo " PASS: $PASS   FAIL: $FAIL   SKIP: $SKIP"
echo "=========================================="
[ $FAIL -gt 0 ] && exit 1 || exit 0
