# Building Blocks - U-Boot Boot Script
# Implements A/B slot selection with fallback and recovery
#
# Build: mkimage -A arm64 -O linux -T script -C none -d boot.cmd boot.scr

# ------------------------------------------------------------------
# 1. Load environment
# ------------------------------------------------------------------
env load

# ------------------------------------------------------------------
# 2. Check for forced recovery (GPIO or env flag)
# ------------------------------------------------------------------
if test "${recovery_mode}" = "1"; then
    echo "[BB] Recovery mode requested (env flag)"
    setenv boot_slot recovery
fi

# ------------------------------------------------------------------
# 3. Determine slot to boot
# ------------------------------------------------------------------
if test "${boot_slot}" != "a" && test "${boot_slot}" != "b"; then
    echo "[BB] Invalid boot_slot='${boot_slot}', defaulting to slot A"
    setenv boot_slot a
    setenv boot_attempt ${boot_limit}
    setenv boot_ok 0
    env save
fi

# ------------------------------------------------------------------
# 4. Check boot_ok from previous boot
# ------------------------------------------------------------------
if test "${boot_ok}" = "0"; then
    # Previous boot did not complete successfully
    if test ${boot_attempt} -le 1; then
        # All attempts exhausted for this slot
        echo "[BB] FATAL: slot ${boot_slot} failed after ${boot_limit} attempts"

        # Switch to alternate slot
        if test "${boot_slot}" = "a"; then
            setenv boot_slot b
        else
            setenv boot_slot recovery
            echo "[BB] Both slots failed! Entering recovery mode."
        fi

        setenv boot_attempt ${boot_limit}
        setenv boot_ok 0
        env save
    else
        # Decrement attempt counter
        setexpr boot_attempt ${boot_attempt} - 1
        env save
        echo "[BB] Retry slot ${boot_slot}, attempt ${boot_attempt}/${boot_limit}"
    fi
else
    # Previous boot was successful, reset counter
    setenv boot_attempt ${boot_limit}
    setenv boot_ok 0
    env save
fi

# ------------------------------------------------------------------
# 5. Recovery mode
# ------------------------------------------------------------------
if test "${boot_slot}" = "recovery"; then
    echo "[BB] Booting recovery..."
    setenv bootargs "console=ttymxc0,115200 root=/dev/mmcblk0p8 rw rootwait panic=5"
    load mmc 0:8 ${kernel_addr_r} /boot/Image
    load mmc 0:8 ${fdt_addr_r}  /boot/imx8mp-ok8mplus-c.dtb
    booti ${kernel_addr_r} - ${fdt_addr_r}
fi

# ------------------------------------------------------------------
# 6. Normal A/B boot
# ------------------------------------------------------------------
if test "${boot_slot}" = "a"; then
    setenv boot_part 4
    setenv root_part 6
else
    setenv boot_part 5
    setenv root_part 7
fi

echo "[BB] Booting slot ${boot_slot} (boot=mmcblk0p${boot_part}, root=mmcblk0p${root_part})"
echo "[BB] Attempt ${boot_attempt}/${boot_limit}"

# Kernel command line
setenv bootargs "console=ttymxc0,115200 \
    root=/dev/mmcblk0p${root_part} rw rootwait panic=5 \
    boot_slot=${boot_slot} \
    rootfstype=ext4 \
    cma=512M"

# Load kernel and device tree
load mmc 0:${boot_part} ${kernel_addr_r} /Image
load mmc 0:${boot_part} ${fdt_addr_r}  /imx8mp-ok8mplus-c.dtb

# Optional: load device tree overlays
if test -e mmc 0:${boot_part} /overlays; then
    fdt addr ${fdt_addr_r}
    # Load display overlay if present
    if load mmc 0:${boot_part} ${loadaddr} /overlays/display.dtbo; then
        fdt apply ${loadaddr}
        echo "[BB] Applied display overlay"
    fi
    # Load camera overlay if present
    if load mmc 0:${boot_part} ${loadaddr} /overlays/camera.dtbo; then
        fdt apply ${loadaddr}
        echo "[BB] Applied camera overlay"
    fi
fi

# Set kernel DTB address
fdt addr ${fdt_addr_r}

booti ${kernel_addr_r} - ${fdt_addr_r}
