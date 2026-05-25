# Building Blocks System Architecture

## Forlinx OK8MPlus-C (NXP i.MX8M Plus) — Complete System Design

---

## 1. Boot Chain

### 1.1 Boot Device (eMMC)

Primary boot device: **on-board eMMC (mmcblk0)** . The i.MX8M Plus Boot ROM reads boot
configuration from eFUSE/GPIO straps, then loads SPL from eMMC boot partition 0.

```
Boot ROM (internal, immutable)
  → SPL (U-Boot SPL, loaded to OCRAM)
    → SCU Firmware (System Controller Unit, loaded by SPL)
    → ATF BL31 (ARM Trusted Firmware, EL3 runtime)
    → U-Boot proper (EL2)
      → Linux Kernel (EL1)
```

### 1.2 ARM Trusted Firmware (ATF / BL31)

- **Repo:** `imx-atf` (NXP fork of ARM Trusted Firmware-A)
- **Role:** EL3 runtime, PSCI (power state coordination), secure monitor calls
- **Binary:** `bl31.bin`, loaded by SPL into secure DRAM region
- **Version:** tracked in `/etc/firmware/atf-version` on rootfs

### 1.3 SCU Firmware (System Controller Unit)

The i.MX8M Plus has an on-chip Cortex-M4 SCU that manages system-level resources:

- **Binary:** `mx8mp-m4-scfw.bin` (loaded by SPL before ATF)
- **Functions:** clock/power gating, DDR frequency scaling, thermal monitoring,
  resource partitioning between A53 and M7 cores
- **Version:** tracked in `/etc/firmware/scfw-version`
- **Update mechanism:** SCU firmware lives in eMMC boot partition alongside SPL,
  updated via `uuu` (NXP Universal Update Utility) or `bb-update` in recovery mode

### 1.4 U-Boot Version

```
U-Boot 2023.04-lf_v2023.04+bb (Apr 10 2025 - 12:00:00 +0000)
```

Version is stamped at build time and queryable at runtime:

```bash
# From Linux userspace
fw_printenv uboot_version

# From U-Boot prompt
=> version
```

The version string is embedded in the U-Boot environment during the Yocto build
and written to the `persist` partition after first successful boot.

### 1.5 U-Boot Environment

Two redundant environment regions on eMMC, stored at fixed offsets in the U-Boot
raw partition (not filesystem-backed):

```
Partition:  mmcblk0p2  →  U-Boot env A  (offset 0x400000, size 0x2000)
Partition:  mmcblk0p3  →  U-Boot env B  (offset 0x600000, size 0x2000)
```

Key environment variables:

```
boot_targets   = mmc0                   # Boot from eMMC
boot_slot      = a                      # Active slot: a | b
boot_ok        = 0                      # Kernel sets to 1 after successful boot
boot_attempt   = 3                      # Remaining attempts before auto-fallback
boot_limit     = 3                      # Max attempts per slot
boot_alt_slot  = b                      # Alternate slot for fallback
uboot_version  = 2023.04-lf_v2023.04+bb
recovery_mode  = 0                      # Set by user or boot_attempt=0
fdt_file       = imx8mp-ok8mplus-c.dtb  # Device tree filename
kernel_image   = Image                  # Kernel image name
```

### 1.6 U-Boot Boot Script (start-up scripts)

The boot script (`boot.scr`) is compiled from `boot.cmd` and stored in the boot
partition of each slot. It implements the A/B selection and fallback logic.

See: `deploy/boot/boot.cmd` — U-Boot script source
See: `deploy/boot/boot.scr` — Compiled script (mkimage)

Key logic:

```
1. Load U-Boot env from eMMC
2. Determine active slot from "boot_slot" env var
3. If boot_attempt <= 0 → enter recovery mode
4. Decrement boot_attempt, save env
5. Load kernel + dtb from boot_<slot> partition
6. Set bootargs with root=<rootfs_slot_part> boot_slot=<slot>
7. Boot kernel
```

### 1.7 U-Boot Commands

Custom U-Boot commands registered via `CONFIG_CMD_BB`:

| Command | Description |
|---------|-------------|
| `bb_slot` | Read/write active boot slot |
| `bb_recovery` | Force boot to recovery partition |
| `bb_factory` | Reset all state, boot recovery |
| `bb_info` | Print slot status, versions, boot counts |
| `bb_update` | Enter update mode (accept image via USB/fastboot) |

Implementation: `deploy/boot/uboot/cmd_bb.c` (to be merged into U-Boot tree)

### 1.8 Boot OK Flag

The `boot_ok` flag is the kernel→bootloader handshake. When the system boots
successfully, a userspace service sets `boot_ok=1` and resets `boot_attempt=boot_limit`.

```
Kernel boots → systemd starts → bb-boot-ok.service runs:
  1. Verify critical services are up (bb-busd, networking, etc.)
  2. fw_setenv boot_ok 1
  3. fw_setenv boot_attempt <boot_limit>
  4. Log boot success to persist partition
```

If `boot_ok` remains 0 at next boot (crash/power loss during startup),
the bootloader's attempt counter is already decremented. After `boot_limit`
failed attempts, the bootloader automatically falls back to the alternate slot.

### 1.9 Start-up Time

Measured and logged at multiple checkpoints:

| Checkpoint | Measurement | Target |
|-----------|-------------|--------|
| Boot ROM → SPL | U-Boot SPL timestamp | < 100 ms |
| SPL → U-Boot proper | U-Boot board_init_r | < 500 ms |
| U-Boot → Kernel handoff | bootstage report | < 2 s total |
| Kernel → init (systemd) | `systemd-analyze` | < 3 s |
| Init → Application ready | bb-boot-ok fired | < 8 s |

The final metric (`boot_to_app_ready_ms`) is logged to the persist partition
on every boot and available via `bb-cli system boottime`.

---

## 2. Partition Scheme

### 2.1 Partition Layout (eMMC, 8 GB)

```
Device         Size     Type      Label              Purpose
────────────────────────────────────────────────────────────────────
mmcblk0boot0   4 MiB    raw       —                  Boot ROM HW partition 0
mmcblk0boot1   4 MiB    raw       —                  Boot ROM HW partition 1

mmcblk0p1      8 MiB    raw       uboot              SPL + U-Boot proper + ATF + SCU FW
mmcblk0p2      8 KiB    raw       uboot-env-a        U-Boot environment A (redundant)
mmcblk0p3      8 KiB    raw       uboot-env-b        U-Boot environment B (redundant)
mmcblk0p4      64 MiB   vfat      boot-a             Kernel + DTB (slot A)
mmcblk0p5      64 MiB   vfat      boot-b             Kernel + DTB (slot B)
mmcblk0p6      1536 MiB ext4      rootfs-a           Root filesystem (slot A)
mmcblk0p7      1536 MiB ext4      rootfs-b           Root filesystem (slot B)
mmcblk0p8      512 MiB  ext4      recovery           Recovery rootfs (minimal)
mmcblk0p9      256 MiB  ext4      persist            Persistent data (A/B updates safe)
mmcblk0p10     1 MiB    raw       manufacturing      Manufacturing data (MAC, S/N, keys)
mmcblk0p11     512 MiB  ext4      log                Dedicated log partition
────────────────────────────────────────────────────────────────────
Total used:    ~4.4 GiB (remainder unallocated for future use)
```

### 2.2 Partition Roles

| Partition | Mount | Writable | Survives Update |
|-----------|-------|----------|-----------------|
| uboot | — | Only in recovery | Manual only |
| uboot-env-* | — | Yes (fw_setenv) | Yes |
| boot-a / boot-b | `/boot` | Inactive: no, Active: rare | Per-slot |
| rootfs-a / rootfs-b | `/` | Yes | Per-slot |
| recovery | — | In recovery mode | Manual only |
| persist | `/persist` | Yes | **Yes** |
| manufacturing | `/mfg` (ro) | No | **Yes** |
| log | `/var/log/persist` | Yes | **Yes** |

### 2.3 Persist Partition (`/persist`)

The persist partition holds data that must survive A/B updates:

```
/persist/
├── machine-id            # Unique machine identity (systemd)
├── ssh/                  # SSH host keys
├── certs/                # TLS client certificates
├── config/               # User/service configuration overrides
│   ├── network.conf      # Static IP, hostname
│   └── blocks.conf       # Per-block configuration
├── mfg-data.bin          # Mirror of manufacturing data (readable format)
├── boot-count.log        # Boot success/failure history (ring buffer, 256 entries)
├── update-log.json       # Update history (last 32 updates)
└── crash/                # Fatal log dumps preserved across reboots
```

### 2.4 Manufacturing Partition (`/mfg`)

Raw binary partition with fixed-layout manufacturing data:

```
Offset  Size    Field
0x000   32      Serial Number (ASCII, null-terminated)
0x020   6       MAC Address (EUI-48, binary)
0x026   2       MAC Address Reserved (for WiFi)
0x028   32      Hardware Revision (ASCII)
0x048   16      Manufacturing Date (ISO 8601, ASCII)
0x058   32      Part Number (ASCII)
0x078   64      Device Certificate Fingerprint (SHA-256 hex)
0x0B8   8       Reserved / CRC32
```

Module: `libbb/bb_persist.h/c` — access to persist and manufacturing data

### 2.5 Double Buffer (A/B Boot Scheme)

The A/B scheme uses two complete sets of boot + rootfs partitions.
Only one slot is "active" at a time.

```
Normal boot flow:

  boot_slot = a
  boot_attempt = 3
  ┌──────────────┐
  │ Kernel boots  │──success──→ boot_ok=1, boot_attempt=3
  │ from boot-a   │
  │ rootfs-a      │──failure──→ boot_ok=0
  └──────────────┘              (power cycle / watchdog)
                                   │
                                   ▼
  boot_slot = a
  boot_attempt = 2
  ┌──────────────┐
  │ Retry boot-a  │──failure──→ boot_attempt = 1 → retry
  └──────────────┘──failure──→ boot_attempt = 0 → FALLBACK
                                   │
                                   ▼
  boot_slot = b
  boot_attempt = 3
  ┌──────────────┐
  │ Boot from     │──success──→ boot_ok=1, system alerts: "slot-a failed"
  │ boot-b        │
  │ rootfs-b      │──failure──→ boot_attempt=0 → RECOVERY MODE
  └──────────────┘
```

The bootloader never increments the slot — it only decrements boot_attempt.
Userspace is responsible for marking boot_ok and resetting boot_attempt.
This ensures a crash at any point before userspace confirmation is treated
as a failed boot.

---

## 3. Kernel & Device Tree

### 3.1 Kernel

- **Source:** Linux 5.4.70 (NXP BSP) → planned migration to 6.1 LTS
- **Config:** `imx_v8_defconfig` + building-blocks fragment (`deploy/kernel/bb-fragment.config`)
- **Image:** `Image` (uncompressed, loaded by U-Boot), stored in `boot-<slot>/Image`
- **Key configs required:**
  - `CONFIG_DM_CRYPT` (dm-verity for rootfs integrity, optional)
  - `CONFIG_SQUASHFS` (recovery rootfs)
  - `CONFIG_EXT4_FS`
  - `CONFIG_FW_ENV_DEVICE` (U-Boot env access from Linux via `fw_printenv`/`fw_setenv`)
  - `CONFIG_PSTORE` (panic log persistence via pstore/ramoops)

### 3.2 Device Tree

- **Primary:** `imx8mp-ok8mplus-c.dtb` (Forlinx board)
- **Overlay mechanism:** U-Boot loads base DTB + optional overlay DTBO files from `boot-<slot>/overlays/`
- **Overlays for:** display panel selection, camera sensor, audio codec, CAN bus
- **Build:** Part of kernel build (`make dtbs`), output to `deploy/dtb/`

### 3.3 Rootfs

- **Base:** Yocto `core-image-minimal` or `core-image-base`
- **Format:** ext4 filesystem image, optionally with dm-verity for integrity
- **Contents:** systemd, libbb blocks, busybox, update agent, no development tools
- **Size target:** < 512 MiB compressed, < 1.5 GiB uncompressed

---

## 4. Update System

### 4.1 Version

Full version tuple stored in `/etc/os-release` and `/persist/current-version`:

```
BUILDING_BLOCKS_VERSION=2.0.0
BUILD_ID=20250525
BUILD_SLOT=a
```

Slot-relative versioning: each slot independently records what version is installed.
After an update, the target slot gets the new version; the source slot retains the
previous version as the fallback.

### 4.2 Update Creation Tools (`tools/bb-update/`)

Updates are self-contained `.bbu` (Building Blocks Update) files — compressed tar
archives with a signed manifest:

```
bb-update create \
    --version 2.0.1 \
    --slot a|b \
    --kernel Image \
    --dtb imx8mp-ok8mplus-c.dtb \
    --rootfs rootfs.ext4 \
    --sign-key /path/to/private.pem \
    --output update-v2.0.1.bbu
```

`.bbu` file structure:

```
update-v2.0.1.bbu
├── manifest.json          # Version, slot, checksums, timestamp
├── manifest.json.sig      # RSA-256 signature of manifest
├── boot.tar.gz            # Kernel + DTB
├── rootfs.tar.gz          # Root filesystem diff or full image
└── post-install.sh        # Optional post-install script (run in target context)
```

### 4.3 Installation Procedure

```
1. bb-update verify   update.bbu
     ├── Check signature against embedded public key(s)
     ├── Validate manifest checksums
     └── Check version > current version

2. bb-update install  update.bbu
     ├── Determine target slot (opposite of current active slot)
     ├── Write boot.tar.gz → boot_<target_slot> partition
     ├── Write rootfs.tar.gz → rootfs_<target_slot> partition
     ├── Set U-Boot env: boot_slot=<target>, boot_attempt=<boot_limit>, boot_ok=0
     ├── Log update event to /persist/update-log.json
     └── Reboot

3. System boots into new slot
     ├── If boot_ok set → update complete, old slot becomes fallback
     └── If boot_ok not set (×3) → automatic rollback to old slot
```

### 4.4 Snapshot Creation

Before an update, the current slot state can be snapshotted for rollback:

```bash
bb-update snapshot create --label "pre-update-v2.0.1"
```

The snapshot captures:
- List of installed packages and versions
- Contents of `/etc` (configuration delta)
- Contents of `/persist` (machine identity and keys)

Snapshots are stored on the **recovery partition** and are also used during
recovery mode to restore a known-good state.

### 4.5 Partition Recovery

If a partition is corrupted (detected via ext4 journal errors or dm-verity
failures), the recovery system can re-image it:

```bash
bb-recovery repair --partition rootfs-a
```

The recovery partition contains a minimal rootfs with all tools needed to:
- Re-format any partition
- Download a fresh image from USB/network
- Restore from a snapshot
- Verify partition integrity (fsck, dm-verity)

---

## 5. Recovery System

### 5.1 Recovery Partition

The recovery partition (`mmcblk0p8`) contains a minimal, independent Linux
system that boots when:

1. Both A and B slots have `boot_attempt = 0`
2. A GPIO/button triggers recovery mode (physical access)
3. A specific U-Boot env flag (`recovery_mode=1`) is set by userspace

Recovery rootfs contents:
```
/bin/busybox           # Swiss army knife
/usr/bin/bb-recovery   # Recovery agent
/usr/bin/fw_setenv     # U-Boot environment tool
/usr/sbin/parted       # Partition manipulation
/usr/sbin/e2fsprogs    # ext4 tools
/usr/bin/wget          # Network download (for remote recovery)
/lib/modules/          # Kernel modules (network, USB, storage)
```

### 5.2 Recovery Agent (`bb-recovery`)

Capabilities:
- **Self-test:** verify all partitions, report status
- **Re-image:** download + flash any partition from USB or network
- **Slot switch:** force boot from alternate slot
- **Factory reset:** restore manufacturing state
- **Log export:** dump persistent logs to USB for offline analysis

### 5.3 Init Scripts

Recovery uses a minimal `/init` (not systemd — a simple busybox-init script)
to keep the recovery rootfs small and robust:

```
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs dev /dev

# Mount persist for crash data access
mount /dev/mmcblk0p9 /persist
mount /dev/mmcblk0p11 /var/log

# Start recovery shell on debug UART + optionally telnet
/sbin/getty -L ttymxc0 115200 vt100 &
bb-recovery --interactive
```

---

## 6. Logging System

### 6.1 Logging Architecture

Three-tier logging system:

| Tier | Destination | Purpose | Retention |
|------|------------|---------|-----------|
| **Functional** | journald (volatile) | Normal operation logs | Ring buffer, 32 MB in RAM |
| **Persistent** | `/var/log/persist/` (log partition) | Functional logs that survive reboot | Rotated, 128 MB max |
| **Fatal/Crash** | `/persist/crash/` (persist partition) | Fatal error + backtrace dumps | Ring buffer, 32 files × 1 MB |

### 6.2 Logfile Location

```
/var/log/persist/          # Mount point for mmcblk0p11 (log partition)
├── syslog.log             # Kernel + system messages (rsyslog or journald export)
├── blocks/                # Per-block logs
│   ├── bb-led.log
│   ├── bb-avgate.log
│   └── bb-busd.log
├── audit/                 # Security-relevant events
│   └── audit.log
└── archive/               # Compressed rotated logs
    ├── syslog.1.gz
    └── syslog.2.gz

/persist/crash/            # Fatal dumps (on persist partition for maximum durability)
├── crash-20250525-120000.log
├── crash-20250525-120000.kdump   # Kernel crash dump (if kdump configured)
└── latest -> crash-20250525-120000.log
```

### 6.3 Fatal Logfile Persistence

When a block crashes (SIGSEGV, SIGABRT, unhandled error):

```
1. Signal handler in bb_block registers the crash
2. Stack trace captured (backtrace() + addr2line offline)
3. Register dump + /proc/self/maps snapshot
4. Written to /persist/crash/crash-<timestamp>.log
5. Kernel panic logs captured via pstore/ramoops (CONFIG_PSTORE)
6. On next boot, bb-crash-report.service detects crash files
   and publishes /dev/system/event {"type":"crash","block":"..."}
```

### 6.4 Functional Log User

Two consumer paths:

1. **Local:** `bb-cli log` reads from journald and persistent logs
2. **Remote:** `bb-logd` (planned) exports logs via syslog/TCP or MQTT bridge

Log level control via bus command:
```bash
bb-cli log level bb-avgate debug    # Set single block to DEBUG
bb-cli log level system info        # Set system-wide to INFO
```

### 6.5 Logging Self-Disclosures

Each block publishes its logging capabilities and current state:

```
Topic: /dev/<block-id>/diagnostics
Payload: {
  "log_level": "info",
  "log_file": "/var/log/persist/blocks/bb-avgate.log",
  "log_size_bytes": 1048576,
  "crash_count": 0,
  "uptime_seconds": 86400,
  "memory_kb": 12340
}
```

This enables automated monitoring: if a block's log size grows abnormally
or crash count increments, the supervision layer can alert or restart.

---

## 7. System Management

### 7.1 Supervision (systemd)

Each block is a systemd service with explicit supervision policies:

```ini
# deploy/system/bb-avgate.service
[Unit]
Description=Building Blocks - AV Gateway
Requires=bb-busd.service
After=bb-busd.service network.target time-sync.target
Wants=time-sync.target

[Service]
Type=notify
ExecStart=/opt/building-blocks/bin/bb-avgate
Restart=on-failure
RestartSec=2s
WatchdogSec=30s
LimitRTTIME=infinity
LimitRTTIME=infinity
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=50
MemoryMax=512M
CPUAffinity=0-1

# Log to journal + forward to persistent log
StandardOutput=journal
StandardError=journal
SyslogIdentifier=bb-avgate

# Crash handler
FailureAction=bb-crash-report@%N.service

[Install]
WantedBy=multi-user.target
```

Supervision matrix:

| Condition | Action |
|-----------|--------|
| Process exit (non-zero) | Restart (RestartSec 2s) |
| Watchdog timeout (30s) | Force kill + restart |
| 5 restarts in 10s | Stop, escalate to recovery hint |
| OOM kill | Log to crash, restart with 2× memory |
| SIGSEGV/SIGABRT | Crash dump, restart, increment crash counter |

### 7.2 Init Scripts (systemd Services)

All systemd units are in `deploy/system/`:

```
deploy/system/
├── bb-busd.service           # Message bus daemon (must start first)
├── bb-boot-ok.service        # Boot handshake: sets boot_ok=1 after all critical services up
├── bb-crash-report@.service  # Template: crash checker for each block
├── bb-logrotate.service      # Daily log rotation timer
├── bb-logrotate.timer
├── bb-update-check.service   # Periodic update availability check
├── bb-update-check.timer
├── bb-time-sync.service      # Time synchronization watchdog
├── bb-avgate.service         # AV Gateway block
├── bb-led.service            # LED controller block (existing)
└── bb-health.service         # System health monitor (publishes /system/health)
```

Startup ordering:

```
bb-busd.service
  ├── bb-led.service (After + Requires bb-busd)
  ├── bb-avgate.service (After + Requires bb-busd + network.target + time-sync.target)
  └── bb-health.service (After bb-busd + all block services)
         │
         └── bb-boot-ok.service (After bb-health)
                └── fw_setenv boot_ok 1
```

### 7.3 Time Synchronization

Required for TLS, logging timestamps, and AV stream timestamp accuracy:

```
Layer 1: Kernel time from RTC (i.MX8MP on-chip SNVS RTC)
Layer 2: systemd-timesyncd (SNTP, pool.ntp.org)
Layer 3: bb-time-sync.service (monitor)
```

`bb-time-sync.service` monitors sync status:
- If NTP is unreachable, falls back to RTC
- If clock jumped > 1 second, publishes `/system/event {"type":"time_jump",...}`
- AV pipeline uses `CLOCK_MONOTONIC` internally, converts to wall-clock only at boundary

### 7.4 Watchdog

Dual-layer watchdog:

```
Hardware: i.MX8MP internal WDOG (already in HAL: bb_hal_wdg)
  ├── System-level: /dev/watchdog (systemd WatchdogSec)
  └── Block-level: each block opens its own watchdog via bb_hal_wdg

Software: systemd WatchdogSec= per service
```

---

## 8. AV Gateway Thread Architecture (Data Plane)

```
┌──────────────────────────────────────────────────────────────┐
│                     bb-avgate process                        │
│                                                              │
│  ┌──────────────────────────────┐   ┌────────────────────┐  │
│  │  Control Thread (systemd)     │   │  bb_block_main_loop │  │
│  │  ├── AF_UNIX bus client       │   │  ├── CMD handler     │  │
│  │  ├── Watchdog ping (30s)      │   │  └── STATE publish   │  │
│  │  └── sd_notify()              │   └────────────────────┘  │
│  └──────────────┬───────────────┘                            │
│                 │ command queue (lock-free SPSC)              │
│  ┌──────────────┴─────────────────────────────────────────┐  │
│  │  Pipeline Manager Thread                                │  │
│  │  ├── Start/stop ingest/encode/stream threads            │  │
│  │  ├── Error handling + pipeline restart                  │  │
│  │  └── Statistics collection                              │  │
│  └──┬────────────┬────────────┬────────────────────────────┤  │
│     │            │            │                            │  │
│  ┌──▼──────┐ ┌──▼──────┐ ┌──▼──────┐ ┌────────────────┐ │  │
│  │ Ingest  │ │ Encode  │ │ Stream  │ │ Display (local) │ │  │
│  │ Thread  │ │ Thread  │ │ Thread  │ │ Thread          │ │  │
│  │ core:0  │ │ core:1  │ │ core:2  │ │ core:3          │ │  │
│  │ V4L2/CSI│ │ VPU H.264│ │ RTSP    │ │ DRM/KMS         │ │  │
│  └──┬──────┘ └──▲──┬───┘ └──▲──────┘ └──▲─────────────┘ │  │
│     │            │  │        │           │               │  │
│     └── raw_pool─┘  │        │           │               │  │
│                     └─ enc_pool ─────────┘               │  │
│                                                          │  │
│  Frame Pools (DMA-BUF backed, zero-copy between threads)  │  │
│  ├── raw_pool: 8 × YUV420 NV12 frames (1920×1080 = 3MB)   │  │
│  └── enc_pool: 16 × H.264 IDR/NALU buffers (512KB each)   │  │
└──────────────────────────────────────────────────────────┘
```

This is the control-plane/data-plane hybrid architecture described in the
previous analysis: systemd manages the process lifecycle (multi-process
isolation), while AV processing runs in threads within `bb-avgate` for
zero-copy frame passing via shared memory pools.

---

## 9. Source Tree (Updated)

```
├── docs/
│   └── system-architecture.md    # This document
├── libbb/
│   ├── bb_types.h/c              # Common types + protocol constants
│   ├── bb_json.h/c               # Minimal JSON parser/writer
│   ├── bb_bus.h/c                # AF_UNIX bus client
│   ├── bb_block.h/c              # Block lifecycle state machine
│   ├── bb_thread.h/c             # Thread abstraction (NEW)
│   ├── bb_pool.h/c               # Frame buffer pool (lock-free queue) (NEW)
│   ├── bb_log.h/c                # Logging subsystem (NEW)
│   ├── bb_persist.h/c            # Persist + manufacturing data access (NEW)
│   ├── bb_recovery.h/c           # Recovery mode interface (NEW)
│   ├── bb_hal_led.h/c            # LED HAL
│   ├── bb_hal_gpio.h/c           # GPIO HAL
│   ├── bb_hal_i2c.h/c            # I2C HAL
│   ├── bb_hal_spi.h/c            # SPI HAL
│   ├── bb_hal_pwm.h/c            # PWM HAL
│   ├── bb_hal_rtc.h/c            # RTC HAL
│   ├── bb_hal_wdg.h/c            # Watchdog HAL
│   └── bb_hal_uart.h/c           # UART HAL
├── blocks/
│   ├── bb-led/main.c             # LED controller block
│   └── bb-avgate/                # AV Gateway block (NEW)
│       ├── main.c                #   Entry point + control thread
│       ├── pipeline.c/h          #   Pipeline manager
│       ├── ingest.c/h            #   V4L2/CSI capture
│       ├── encode.c/h            #   Hardware encoding (VPU)
│       ├── stream.c/h            #   RTSP/RTMP output
│       └── display.c/h           #   Local DRM/KMS preview
├── tools/
│   ├── bb-busd.c                 # Message bus daemon
│   ├── bb-cli.c                  # CLI debug tool
│   ├── bb-hal-test.c             # HAL validation tool
│   └── bb-update/                # Update management (NEW)
│       ├── bb-update.c           #   Update creation + installation
│       ├── manifest.c/h          #   Manifest generation + signing
│       └── verify.c/h            #   Signature + checksum verification
├── deploy/
│   ├── boot/
│   │   ├── boot.cmd              #   U-Boot boot script source
│   │   ├── boot.scr              #   Compiled boot script
│   │   ├── uboot-env.txt         #   Default U-Boot environment
│   │   └── recovery-init         #   Recovery mode /init script
│   ├── system/
│   │   ├── bb-boot-ok.service
│   │   ├── bb-avgate.service
│   │   ├── bb-crash-report@.service
│   │   ├── bb-health.service
│   │   ├── bb-logrotate.service
│   │   ├── bb-logrotate.timer
│   │   ├── bb-time-sync.service
│   │   └── bb-update-check.service
│   │   └── bb-update-check.timer
│   ├── bb-busd.service           # (existing)
│   └── bb-led.service            # (existing)
├── examples/
│   └── led_example.c
├── Makefile                      # (updated)
└── .gitignore
```

---

## 10. Build & Deploy

### Build Targets (updated Makefile)

```
make all        # Build all blocks, tools, and library objects
make bb-avgate  # Build only the AV Gateway block
make cross      # Cross-compile with aarch64-linux-gnu-gcc
make update-tool # Build the update creation tool (host-native only)
make deploy     # SCP all binaries + units to target board
make bbu        # Create a .bbu update package
```

### Deployment Flow

```
Development                 Target Board
-----------                 ------------
make cross
make bbu         ──SCP──→   bb-update install update.bbu
                            systemctl reboot
                            [boots into new slot]
                            [sets boot_ok=1 if healthy]
```

---

## Appendix: Key Design Decisions Summary

| # | Element | Decision |
|---|---------|----------|
| 1 | Boot device | eMMC, with fallback to SD card via Boot ROM strap |
| 2 | Boot scripts | U-Boot `boot.scr` (mkimage-compiled), per-slot |
| 3 | Partitioning | A/B dual-slot + recovery + persist + log (11 partitions) |
| 4 | Double buffer | A/B scheme with boot_attempt counter, not GRUB/snapper |
| 5 | U-Boot env | Dual redundant regions on eMMC, accessed via fw_printenv/setenv |
| 6 | boot_ok flag | Userspace → bootloader handshake via fw_setenv |
| 7 | Start-up time | Measured at 5 checkpoints, logged to persist partition |
| 8 | U-Boot version | Embedded in env + /etc/firmware/ |
| 9 | U-Boot commands | Custom `bb_*` commands for slot/recovery/update control |
| 10 | ATF | NXP imx-atf, BL31, PSCI provider |
| 11 | SCU firmware | mx8mp-m4-scfw.bin, loaded by SPL |
| 12 | Recovery | Standalone partition with minimal rootfs + busybox |
| 13 | Update tools | `bb-update` CLI for .bbu creation, signing, installation |
| 14 | Installation | Off-slot update + reboot + boot_ok confirmation |
| 15 | Partition recovery | `bb-recovery repair` from recovery mode |
| 16 | Logging | Three-tier: journald (volatile) → log partition → crash dump |
| 17 | Snapshot | Pre-update state capture on recovery partition |
| 18 | Fatal log persistence | /persist/crash/ ring buffer, 32 files, includes backtrace |
| 19 | Functional log user | bb-cli log + remote syslog/MQTT export |
| 20 | Logfile location | Dedicated mmcblk0p11 partition, /var/log/persist/ |
| 21 | Logging self-disclosure | Each block publishes /dev/<id>/diagnostics via bus |
| 22 | Kernel | Linux 5.4.70 (NXP BSP), pstore + dm-verity + fw_env |
| 23 | Device tree | imx8mp-ok8mplus-c.dtb + overlay mechanism |
| 24 | Rootfs | Yocto ext4, per-slot, < 1.5 GiB |
| 25 | Version | /etc/os-release + /persist/current-version, per-slot |
| 26 | Update | Signed .bbu packages, off-slot install, auto-rollback |
| 27 | Init scripts | systemd units with explicit dependency ordering |
| 28 | Supervision | systemd with WatchdogSec, Restart=on-failure, FailureAction |
| 30 | Time sync | SNVS RTC → systemd-timesyncd (SNTP) → bb-time-sync monitor |
| 31 | Persist partition | mmcblk0p9, ext4, survives A/B updates |
| 32 | Manufacturing data | mmcblk0p10, raw binary, fixed-layout, read-only mount |
