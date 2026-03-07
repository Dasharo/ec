# SPDX-License-Identifier: GPL-3.0-only

ec-y += ec.c
ec-y += adc.c
ec-$(CONFIG_BUS_ESPI) += espi.c
ec-y += gpio.c
ec-y += i2c.c
ec-y += kbc.c
ec-y += pmc.c
ec-y += ps2.c
ec-y += signature.c

ifeq ($(CONFIG_EC_ITE_IT8587E), y)
CFLAGS+=-DCONFIG_EC_ITE_IT8587E=1
# SRAM is 4096 bytes, but SRAM at address 2048 is used for scratch ROM
SRAM_SIZE=2048
else ifeq ($(CONFIG_EC_ITE_IT5570E), y)
CFLAGS+=-DCONFIG_EC_ITE_IT5570E=1
# SRAM is 6144 bytes, only 4096 bytes are mapped at address 0. Region at
# 0x0E00-0x0FFF is used for AP communication, so the safe XSEG limit is
# 0x0E00 = 3584 bytes (was 2048 to match IT8587E, but IT5570E-only builds
# can use the full safe range).
SRAM_SIZE=3584
else
$(error Unsupported EC)
endif

ARCH=8051

# 64 KB is the max without banking
CODE_SIZE=65536
