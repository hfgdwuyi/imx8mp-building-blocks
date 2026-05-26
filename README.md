# Building Blocks for Forlinx OK8MPlus-C (i.MX8M Plus)

Modular, composable service framework for the **Forlinx OK8MPlus-C** embedded Linux development board (NXP i.MX8M Plus Cortex-A53).

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  bb-cli (C)  │  REST API  │  MQTT Bridge  │  ...        │
├─────────────────────────────────────────────────────────┤
│                    blocks/ (产品组件)                     │
│  bb-led (LED控制)  │  bb-audio (对讲机)  │  ...          │
├─────────────────────────────────────────────────────────┤
│                 middleware/ (流式中间件)                   │
│  bb_audio_stream (采集→编解码→播放管线)                   │
├─────────────────────────────────────────────────────────┤
│                   libbb (核心库+框架)                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │ Bus IPC  │ │  Config  │ │  Block   │ │  Thread  │  │
│  │ (AF_UNIX)│ │  (JSON)  │ │ Lifecycle│ │  Pool    │  │
│  │          │ │  Persist │ │ Recovery │ │  Log     │  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘  │
├─────────────────────────────────────────────────────────┤
│                    hal/ (硬件抽象层)                       │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐        │
│  │ LED  │ │ GPIO │ │ I2C  │ │ SPI  │ │ PWM  │        │
│  │ RTC  │ │ WDG  │ │ UART │ │Audio │ │Display│        │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘        │
├─────────────────────────────────────────────────────────┤
│              Linux Kernel (sysfs / dev / ioctl)           │
└─────────────────────────────────────────────────────────┘
```

## 分层原则

| 层 | 目录 | 职责 | 示例 |
|----|------|------|------|
| **HAL** | `hal/` | 封装内核接口，提供统一 C API | `bb_hal_audio.c` — 纯 ALSA ioctl |
| **中间件** | `middleware/` | 流式数据管线：线程调度、缓冲、编解码 | `bb_audio_stream.c` — 3线程全双工 |
| **核心库** | `libbb/` | 通用基础件 + 应用框架 | thread, pool, log, json, bus, block |
| **业务组件** | `blocks/` | 独立守护进程，通过消息总线协作 | `bb-led` — LED 控制器 |
| **工具** | `tools/` | 开发调试、测试验证 | `bb-hal-test` — HAL 综合验证 |

**判断标准**：简单外设只需 HAL（LED/GPIO/I2C），流式外设需要 HAL + 中间件（音频/视频），有独立运行价值的才做成 block。

## Key Design Decisions

- **Zero runtime dependencies** — statically linked binaries, no Python/Node required
- **三层分离** — HAL 封装硬件细节，middleware 管理流式管线，libbb 提供通用能力
- **HAL 只做硬件抽象** — 不涉及线程、不涉及 buffer 管理，调用者决定一切
- **Unix domain socket bus** — 轻量 IPC，text-based protocol (PUB/SUB/UNSUB/PING)
- **~300KB per service** — static linking, no runtime overhead

## Project Structure

```
├── hal/                        # 硬件抽象层 (10 modules)
│   ├── bb_hal_audio.c/h        # ALSA PCM (NAU8822 codec)
│   ├── bb_hal_display.c/h      # DRM/KMS display (DSI/LVDS/HDMI)
│   ├── bb_hal_led.c/h          # LED (sysfs)
│   ├── bb_hal_gpio.c/h         # GPIO v2 (sysfs)
│   ├── bb_hal_i2c.c/h          # I2C (/dev/i2c-X)
│   ├── bb_hal_spi.c/h          # SPI (/dev/spidevX.Y)
│   ├── bb_hal_pwm.c/h          # PWM (sysfs pwmchip)
│   ├── bb_hal_rtc.c/h          # RTC (/dev/rtcX)
│   ├── bb_hal_wdg.c/h          # Watchdog (/dev/watchdogX)
│   └── bb_hal_uart.c/h         # UART (ttymxc)
├── middleware/                  # 流式中间件
│   └── bb_audio_stream.c/h     # 全双工音频管线 (capture→codec→playback)
├── libbb/                       # 核心库 + 应用框架
│   ├── bb_types.h               # Common types & protocol constants
│   ├── bb_thread.c/h            # POSIX thread wrapper (RT + CPU affinity)
│   ├── bb_pool.c/h              # Zero-copy frame pool (SPSC + MPSC)
│   ├── bb_log.c/h               # Structured logging (file/stderr/syslog)
│   ├── bb_json.c/h              # Minimal JSON parser/writer (zero-alloc)
│   ├── bb_bus.c/h               # Bus client (AF_UNIX socket IPC)
│   ├── bb_block.c/h             # Block lifecycle state machine
│   ├── bb_board.h               # Board pinmux definitions
│   ├── bb_persist.c/h           # Persistent config storage
│   └── bb_recovery.c/h          # Crash recovery + factory reset
├── blocks/                      # 业务组件 (product services)
│   └── bb-led/main.c            # LED controller block
├── services/                    # 系统守护进程
│   └── bb-busd.c                # Message bus daemon
├── tools/                       # 开发工具
│   ├── bb-hal-test.c            # HAL 综合验证 (I2C/SPI/GPIO/LED/PWM/RTC/WDG/UART)
│   ├── bb-audio-test.c          # 音频 HAL 测试 (playback/capture)
│   ├── bb-audio-loopback.c      # 音频回环测试 (全双工 PCM 直通)
│   ├── bb-display-test.c        # 显示测试 (color bars)
│   ├── bb-cli.c                 # 命令行调试工具
│   └── bb-update/               # OTA 更新工具
├── deploy/                      # systemd 服务文件 + boot 脚本
├── Makefile                     # VPATH-driven unified build
└── .gitignore
```

## Build

```bash
# Cross-compile for Forlinx OK8MPlus-C (aarch64)
make cross-forlinx

# Cross-compile for NXP i.MX8MP EVK
make cross

# Native on the board
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

# Audio loopback test (speak into mic, hear yourself)
bb-audio-loopback 5                  # 5-second PCM passthrough
```

## Message Protocol

```
PUB /dev/bb-led/cmd {"cmd":"blink","on_ms":200,"off_ms":200}
SUB /dev/bb-led/#
PING → PONG
```

## Audio Pipeline

```
capture_thread (CPU0, SCHED_FIFO)  →  SPSC pool  →  codec_thread (CPU1)  →  MPSC pool  →  playback_thread (CPU2)
    bb_hal_audio read                  4 frames        passthrough/opus        4 frames        bb_hal_audio write
```

- **PCM passthrough** (default): capture → memcpy → playback, <50ms latency
- **Pluggable codec**: `bb_audio_codec_ops_t` — future Opus encode/decode
- **xrun recovery**: auto-recover on EPIPE, error on 10 consecutive xruns
- **Zero-copy**: `bb_pool` SPSC (lock-free) + MPSC (mutex+condvar)

## Hardware

| Component | Detail |
|-----------|--------|
| **Board** | Forlinx OK8MPlus-C |
| **SoC** | NXP i.MX8M Plus (4x A53, 4GB RAM) |
| **Codec** | NAU8822 (ALSA card 2, I2S) |
| **Display** | DSI 1024x600 / LVDS / HDMI |
| **WiFi/BT** | NXP 88W8987 (SDIO + UART) |
| **OS** | Linux 5.4.70 (Yocto-based) |

## License

MIT
