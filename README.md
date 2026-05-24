# Building Blocks for Forlinx OK8MPlus-C (i.MX8M Plus)

Modular, composable service framework for the **Forlinx OK8MPlus-C** embedded Linux development board (NXP i.MX8M Plus Cortex-A53).

## Architecture

```
┌────────────────────────────────────────────────────┐
│                  Application Layer                  │
│  bb-cli (C)  │  REST API  │  MQTT Bridge  │  ...   │
├────────────────────────────────────────────────────┤
│                   libbb (C, static)                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │ Bus IPC  │ │  Config  │ │  Block   │           │
│  │ (AF_UNIX)│ │  (JSON)  │ │ Lifecycle│           │
│  └──────────┘ └──────────┘ └──────────┘           │
├────────────────────────────────────────────────────┤
│                 HAL (bb_hal_*.c)                    │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │ LED  │ │ GPIO │ │ I2C  │ │ SPI  │ │ PWM  │   │
│  │ RTC  │ │ WDG  │ │ UART │                         │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘   │
├────────────────────────────────────────────────────┤
│           Linux Kernel (sysfs / dev / ioctl)        │
└────────────────────────────────────────────────────┘
```

## Key Design Decisions

- **Zero runtime dependencies** — statically linked binaries, no Python/Node required
- **HAL abstraction** — hardware access isolated in `libbb/bb_hal_*.c`, blocks never touch `/sys` directly
- **Unix domain socket bus** — lightweight IPC, text-based protocol (PUB/SUB/UNSUB/PING), compatible with MQTT topic semantics
- **systemd lifecycle** — each block is an independent systemd service
- **~300KB total memory** for bus + LED block vs ~10MB for equivalent Python version

## Project Structure

```
├── libbb/                  # Core static library
│   ├── bb_types.h          # Common types & protocol constants
│   ├── bb_json.h/c         # Minimal JSON parser/writer (zero-alloc)
│   ├── bb_bus.h/c          # Bus client (AF_UNIX socket IPC)
│   ├── bb_block.h/c        # Block lifecycle state machine
│   ├── bb_hal_led.h/c      # LED HAL (sysfs abstraction)
│   ├── bb_hal_gpio.h/c     # GPIO HAL (sysfs export/value/edge)
│   ├── bb_hal_i2c.h/c      # I2C HAL (/dev/i2c-X, ioctl)
│   ├── bb_hal_spi.h/c      # SPI HAL (/dev/spidevX.Y, ioctl)
│   ├── bb_hal_pwm.h/c      # PWM HAL (sysfs pwmchip)
│   ├── bb_hal_rtc.h/c      # RTC HAL (/dev/rtcX, ioctl)
│   ├── bb_hal_wdg.h/c      # Watchdog HAL (/dev/watchdogX)
│   └── bb_hal_uart.h/c     # UART HAL (ttymxc, termios)
├── blocks/
│   └── bb-led/main.c       # LED controller block
├── tools/
│   ├── bb-busd.c           # Message bus daemon
│   ├── bb-cli.c            # Command-line debug tool
│   └── bb-hal-test.c       # HAL validation tool (I2C/SPI/GPIO)
├── examples/
│   └── led_example.c       # External app using HAL directly
├── deploy/
│   ├── bb-busd.service     # systemd unit: bus daemon
│   └── bb-led.service      # systemd unit: LED controller
├── Makefile                # Build (native or cross-compile)
└── .gitignore
```

## Build

```bash
# Cross-compile for i.MX8MP (aarch64)
make CC=aarch64-linux-gnu-gcc

# Or native on the board
make

# Deploy to board at 192.168.0.232
make deploy
```

## Quick Start (on the board)

```bash
# Start services
systemctl start bb-busd bb-led

# Check status
bb-cli ping                          # → PONG

# Control LED
bb-cli led blink on_ms 100 off_ms 100 count 5   # Fast blink 5 times
bb-cli led solid state on                        # Solid on
bb-cli led heartbeat                             # Restore kernel heartbeat

# Subscribe to events
bb-cli sub '/dev/bb-led/#'          # Watch all LED events live
```

## Message Protocol

```
PUB /dev/bb-led/cmd {"cmd":"blink","on_ms":200,"off_ms":200}
SUB /dev/bb-led/#
PING → PONG
```

## Hardware

| Component | Detail |
|-----------|--------|
| **Board** | Forlinx OK8MPlus-C |
| **SoC** | NXP i.MX8M Plus (4x A53, 4GB RAM) |
| **WiFi/BT** | NXP 88W8987 (SDIO + UART) |
| **OS** | Linux 5.4.70 (Yocto-based) |

## License

MIT
