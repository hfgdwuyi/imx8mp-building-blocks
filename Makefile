# Building Blocks - Makefile
# Target: aarch64 (i.MX8MP) or native for development

CC       ?= gcc
CFLAGS   := -std=c11 -Wall -Wextra -Os -D_GNU_SOURCE
LDFLAGS  := -lpthread

# If cross-compiling, set CC to aarch64-linux-gnu-gcc
# e.g. make CC=aarch64-linux-gnu-gcc

BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj

# ---- Library objects ----
LIBBB_SRCS := libbb/bb_json.c libbb/bb_bus.c libbb/bb_block.c libbb/bb_hal_led.c
LIBBB_OBJS := $(patsubst libbb/%.c,$(OBJ_DIR)/%.o,$(LIBBB_SRCS))

# ---- Targets ----
TARGETS := $(BIN_DIR)/bb-busd $(BIN_DIR)/bb-led $(BIN_DIR)/bb-cli

.PHONY: all clean install deploy

all: $(TARGETS)
	@echo "Build complete:"
	@ls -lh $(BIN_DIR)/

# Bus daemon
$(BIN_DIR)/bb-busd: tools/bb-busd.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# LED block
$(BIN_DIR)/bb-led: blocks/bb-led/main.c $(LIBBB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Ilibbb -o $@ $^ $(LDFLAGS)

# CLI tool
$(BIN_DIR)/bb-cli: tools/bb-cli.c $(OBJ_DIR)/bb_json.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -Ilibbb -o $@ $^ $(LDFLAGS)

# Library objects
$(OBJ_DIR)/%.o: libbb/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -Ilibbb -c -o $@ $<

# Directories
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Deploy to i.MX8 board
deploy: all
	@echo "Deploying to 192.168.0.232..."
	ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa root@192.168.0.232 \
		"mkdir -p /opt/building-blocks/bin"
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		$(BIN_DIR)/* root@192.168.0.232:/opt/building-blocks/bin/
	scp -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
		deploy/*.service root@192.168.0.232:/etc/systemd/system/
	ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa root@192.168.0.232 \
		"chmod +x /opt/building-blocks/bin/* \
		 && ln -sf /opt/building-blocks/bin/bb-cli /usr/bin/bb-cli \
		 && ln -sf /opt/building-blocks/bin/bb-busd /usr/bin/bb-busd \
		 && systemctl daemon-reload \
		 && systemctl enable bb-busd bb-led \
		 && systemctl restart bb-busd bb-led"
	@echo "Deploy complete."

# Cross-compile for aarch64
cross: CC = aarch64-linux-gnu-gcc
cross:
	@$(MAKE) CC=$(CC) all

clean:
	rm -rf $(BUILD_DIR)
