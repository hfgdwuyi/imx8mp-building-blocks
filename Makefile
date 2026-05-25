# Building Blocks - Makefile
# Target: aarch64 (i.MX8MP) or native for development
#
# External apps link individual .o files, e.g.:
#   aarch64-linux-gnu-gcc -Ilibbb -o my_app my_app.c \
#       build/obj/bb_hal_led.o -static -lpthread

CC       ?= gcc
CFLAGS   := -std=c11 -Wall -Wextra -Os -D_GNU_SOURCE -Ilibbb -Itools/bb-update
LDFLAGS  := -static -lpthread

BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj

# ---- Individual module objects (link what you need) ----
OBJ_bb_block    := $(OBJ_DIR)/bb_block.o
OBJ_bb_bus      := $(OBJ_DIR)/bb_bus.o
OBJ_bb_json     := $(OBJ_DIR)/bb_json.o
OBJ_bb_thread   := $(OBJ_DIR)/bb_thread.o
OBJ_bb_pool     := $(OBJ_DIR)/bb_pool.o
OBJ_bb_log      := $(OBJ_DIR)/bb_log.o
OBJ_bb_persist  := $(OBJ_DIR)/bb_persist.o
OBJ_bb_recovery := $(OBJ_DIR)/bb_recovery.o
OBJ_bb_hal_led  := $(OBJ_DIR)/bb_hal_led.o
OBJ_bb_hal_gpio := $(OBJ_DIR)/bb_hal_gpio.o
OBJ_bb_hal_i2c  := $(OBJ_DIR)/bb_hal_i2c.o
OBJ_bb_hal_spi  := $(OBJ_DIR)/bb_hal_spi.o
OBJ_bb_hal_pwm  := $(OBJ_DIR)/bb_hal_pwm.o
OBJ_bb_hal_rtc  := $(OBJ_DIR)/bb_hal_rtc.o
OBJ_bb_hal_wdg  := $(OBJ_DIR)/bb_hal_wdg.o
OBJ_bb_hal_uart := $(OBJ_DIR)/bb_hal_uart.o
OBJ_bb_update   := $(OBJ_DIR)/bb_update.o

# All libbb objects (for deploy)
LIBBB_OBJS := $(OBJ_bb_block) $(OBJ_bb_bus) $(OBJ_bb_json) \
              $(OBJ_bb_thread) $(OBJ_bb_pool) $(OBJ_bb_log) \
              $(OBJ_bb_persist) $(OBJ_bb_recovery) \
              $(OBJ_bb_hal_led) $(OBJ_bb_hal_gpio) $(OBJ_bb_hal_i2c) $(OBJ_bb_hal_spi) \
              $(OBJ_bb_hal_pwm) $(OBJ_bb_hal_rtc) $(OBJ_bb_hal_wdg) $(OBJ_bb_hal_uart)

# All new core objects (thread, pool, log, persist, recovery)
CORE_OBJS := $(OBJ_bb_thread) $(OBJ_bb_pool) $(OBJ_bb_log) $(OBJ_bb_persist) $(OBJ_bb_recovery)

# ---- Targets ----
TARGETS := $(BIN_DIR)/bb-busd $(BIN_DIR)/bb-led $(BIN_DIR)/bb-cli $(BIN_DIR)/bb-hal-test \
           $(BIN_DIR)/bb-update

.PHONY: all clean deploy cross bbu

all: $(TARGETS)
	@echo "Build complete:"
	@ls -lh $(BIN_DIR)/

# ---- Bus daemon (standalone, no libbb) ----
$(BIN_DIR)/bb-busd: tools/bb-busd.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# ---- LED block: links block+bus+json+log+hal_led ----
$(BIN_DIR)/bb-led: blocks/bb-led/main.c $(OBJ_bb_block) $(OBJ_bb_bus) $(OBJ_bb_json) $(OBJ_bb_log) $(OBJ_bb_hal_led) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- CLI tool: json+persist (for boot-time/serial queries) ----
$(BIN_DIR)/bb-cli: tools/bb-cli.c $(OBJ_bb_json) $(OBJ_bb_persist) $(OBJ_bb_log) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- HAL test tool: needs all HAL modules ----
$(BIN_DIR)/bb-hal-test: tools/bb-hal-test.c $(OBJ_bb_hal_i2c) $(OBJ_bb_hal_spi) $(OBJ_bb_hal_gpio) $(OBJ_bb_hal_led) $(OBJ_bb_hal_pwm) $(OBJ_bb_hal_rtc) $(OBJ_bb_hal_wdg) $(OBJ_bb_hal_uart) $(OBJ_bb_log) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- Update tool: update logic + persist + recovery ----
$(BIN_DIR)/bb-update: tools/bb-update/main.c $(OBJ_bb_update) $(OBJ_bb_persist) $(OBJ_bb_recovery) $(OBJ_bb_json) $(OBJ_bb_log) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- Compile individual library modules ----
$(OBJ_DIR)/%.o: libbb/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Compile update module (separate include path) ----
$(OBJ_DIR)/bb_update.o: tools/bb-update/bb_update.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- Directories ----
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# ---- Deploy to i.MX8 board ----
deploy: all
	@echo "Deploying to 192.168.0.232..."
	ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa root@192.168.0.232 \
		"mkdir -p /opt/building-blocks/bin /opt/building-blocks/include /opt/building-blocks/obj"
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		$(BIN_DIR)/* root@192.168.0.232:/opt/building-blocks/bin/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		$(LIBBB_OBJS) root@192.168.0.232:/opt/building-blocks/obj/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		libbb/*.h root@192.168.0.232:/opt/building-blocks/include/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		deploy/*.service root@192.168.0.232:/etc/systemd/system/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		deploy/system/*.service deploy/system/*.timer root@192.168.0.232:/etc/systemd/system/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		deploy/boot/boot.cmd deploy/boot/boot.scr root@192.168.0.232:/opt/building-blocks/boot/
	ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa root@192.168.0.232 \
		"chmod +x /opt/building-blocks/bin/* \
		 && ln -sf /opt/building-blocks/bin/bb-cli /usr/bin/bb-cli \
		 && ln -sf /opt/building-blocks/bin/bb-busd /usr/bin/bb-busd \
		 && ln -sf /opt/building-blocks/bin/bb-update /usr/bin/bb-update \
		 && systemctl daemon-reload \
		 && systemctl enable bb-busd bb-led bb-boot-ok bb-health bb-logrotate.timer bb-time-sync bb-update-check.timer \
		 && systemctl restart bb-busd bb-led"
	@echo "Deploy complete."

# Cross-compile for aarch64
cross: CC = aarch64-linux-gnu-gcc
cross:
	@$(MAKE) CC=$(CC) CFLAGS="$(CFLAGS) -DBOARD_NXP_IMX8MP_EVK" all

# Cross-compile for Forlinx OK8MPlus-C
cross-forlinx: CC = aarch64-linux-gnu-gcc
cross-forlinx:
	@$(MAKE) CC=$(CC) CFLAGS="$(CFLAGS) -DBOARD_FORLINX_OK8MPC" all

# Create a .bbu update package (runs on host)
bbu: all
	@echo "Creating update package..."
	./build/bin/bb-update create \
		--from "1.0.0" \
		--to "2.0.0" \
		--slot "=" \
		--product "$(BB_PRODUCT)" \
		--rootfs build/rootfs.tar.gz \
		--boot build/boot.tar.gz \
		build/update.bbu

clean:
	rm -rf $(BUILD_DIR)
