# Building Blocks - Makefile
# Target: aarch64 (i.MX8MP) or native for development
#
# External apps link individual .o files, e.g.:
#   aarch64-linux-gnu-gcc -Ilibbb -o my_app my_app.c \
#       build/obj/bb_hal_led.o -static -lpthread

CC       ?= gcc
CFLAGS   := -std=c11 -Wall -Wextra -Os -D_GNU_SOURCE -Ilibbb
LDFLAGS  := -static -lpthread

BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj

# ---- Individual module objects (link what you need) ----
OBJ_bb_block    := $(OBJ_DIR)/bb_block.o
OBJ_bb_bus      := $(OBJ_DIR)/bb_bus.o
OBJ_bb_json     := $(OBJ_DIR)/bb_json.o
OBJ_bb_hal_led  := $(OBJ_DIR)/bb_hal_led.o
OBJ_bb_hal_gpio := $(OBJ_DIR)/bb_hal_gpio.o
OBJ_bb_hal_i2c  := $(OBJ_DIR)/bb_hal_i2c.o
OBJ_bb_hal_spi  := $(OBJ_DIR)/bb_hal_spi.o

# All libbb objects (for deploy)
LIBBB_OBJS := $(OBJ_bb_block) $(OBJ_bb_bus) $(OBJ_bb_json) \
              $(OBJ_bb_hal_led) $(OBJ_bb_hal_gpio) $(OBJ_bb_hal_i2c) $(OBJ_bb_hal_spi)

# ---- Targets ----
TARGETS := $(BIN_DIR)/bb-busd $(BIN_DIR)/bb-led $(BIN_DIR)/bb-cli $(BIN_DIR)/bb-hal-test

.PHONY: all clean deploy cross

all: $(TARGETS)
	@echo "Build complete:"
	@ls -lh $(BIN_DIR)/

# ---- Bus daemon (standalone, no libbb) ----
$(BIN_DIR)/bb-busd: tools/bb-busd.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# ---- LED block: links block+bus+json+hal_led ----
$(BIN_DIR)/bb-led: blocks/bb-led/main.c $(OBJ_bb_block) $(OBJ_bb_bus) $(OBJ_bb_json) $(OBJ_bb_hal_led) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- CLI tool: only needs json ----
$(BIN_DIR)/bb-cli: tools/bb-cli.c $(OBJ_bb_json) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- HAL test tool: needs i2c+spi+gpio ----
$(BIN_DIR)/bb-hal-test: tools/bb-hal-test.c $(OBJ_bb_hal_i2c) $(OBJ_bb_hal_spi) $(OBJ_bb_hal_gpio) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- Compile individual library modules ----
$(OBJ_DIR)/%.o: libbb/%.c | $(OBJ_DIR)
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
