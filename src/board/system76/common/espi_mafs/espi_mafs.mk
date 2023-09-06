# SPDX-License-Identifier: GPL-3.0-or-later

espi_mafs-y += main.c

# Set espi_mafs ROM parameters
ESPI_MAFS_OFFSET=3072
ESPI_MAFS_SIZE=1024
CFLAGS+=-DESPI_MAFS_OFFSET=$(ESPI_MAFS_OFFSET) -DESPI_MAFS_SIZE=$(ESPI_MAFS_SIZE)

# Copy parameters to use when compiling espi_mafs ROM
ESPI_MAFS_INCLUDE=$(INCLUDE)
ESPI_MAFS_CFLAGS=$(CFLAGS)

# Include espi_mafs source.
ESPI_MAFS_DIR=$(SYSTEM76_COMMON_DIR)/espi_mafs
# Note: main.c *must* be first to ensure that espi_mafs_start is at the correct address
ESPI_MAFS_INCLUDE += $(ESPI_MAFS_DIR)/espi_mafs.mk
ESPI_MAFS_CFLAGS+=-I$(ESPI_MAFS_DIR)/include -D__ESPI_MAFS__
ESPI_MAFS_SRC += $(foreach src, $(espi_mafs-y), $(ESPI_MAFS_DIR)/$(src))

ESPI_MAFS_SRC += $(BOARD_DIR)/gpio.c
ESPI_MAFS_SRC += src/ec/ite/gpio.c

ESPI_MAFS_BUILD=$(BUILD)/espi_mafs
ESPI_MAFS_OBJ=$(sort $(patsubst src/%.c,$(ESPI_MAFS_BUILD)/%.rel,$(ESPI_MAFS_SRC)))
ESPI_MAFS_CC=\
	sdcc \
	-mmcs51 \
	-MMD \
	--model-large \
	--opt-code-size \
	--acall-ajmp \
	--code-loc $(ESPI_MAFS_OFFSET) \
	--code-size $(ESPI_MAFS_SIZE) \
	--Werror

# Convert from binary file to C header
$(BUILD)/include/espi_mafs.h: $(ESPI_MAFS_BUILD)/espi_mafs.rom
	@echo "  XXD       $(subst $(obj)/,,$@)"
	mkdir -p $(@D)
	xxd -include < $< > $@

# Convert from Intel Hex file to binary file
$(ESPI_MAFS_BUILD)/espi_mafs.rom: $(ESPI_MAFS_BUILD)/espi_mafs.ihx
	@echo "  OBJCOPY   $(subst $(obj)/,,$@)"
	mkdir -p $(@D)
	objcopy -I ihex -O binary $< $@

# Link object files into Intel Hex file
$(ESPI_MAFS_BUILD)/espi_mafs.ihx: $(ESPI_MAFS_OBJ)
	@echo "  LINK      $(subst $(obj)/,,$@)"
	mkdir -p $(@D)
	$(ESPI_MAFS_CC) -o $@ $^

# Compile C files into object files
$(ESPI_MAFS_OBJ): $(ESPI_MAFS_BUILD)/%.rel: src/%.c $(ESPI_MAFS_INCLUDE)
	@echo "  CC        $(subst $(obj)/,,$@)"
	mkdir -p $(@D)
	$(ESPI_MAFS_CC) $(ESPI_MAFS_CFLAGS) -o $@ -c $<

# Include espi_mafs header in main firmware
CFLAGS+=-I$(BUILD)/include
LDFLAGS+=-Wl -g_espi_switch_to_mafs=$(ESPI_MAFS_OFFSET)
INCLUDE+=$(BUILD)/include/espi_mafs.h
