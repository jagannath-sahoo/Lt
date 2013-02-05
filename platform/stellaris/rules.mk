LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

# ROMBASE, MEMBASE, and MEMSIZE are required for the linker script

ARCH := arm

# TI's driverlib wants this
MODULE_COMPILEFLAGS += -Dgcc

ifeq ($(STELLARIS_CHIP),LM4F120H5QR)
MEMSIZE ?= 32768
MEMBASE := 0x20000000
ROMBASE := 0x00000000
ARM_CPU := cortex-m3
# should this be here?
MODULE_DEFINES += TARGET_IS_BLIZZARD_RA1
endif

MODULE_DEFINES += PART_$(STELLARIS_CHIP)

ifeq ($(MEMSIZE),)
$(error need to define MEMSIZE)
endif

INCLUDES += \
	-I$(LOCAL_DIR)/include \
	-I$(LOCAL_DIR)/ti

MODULE_SRCS += \
	$(LOCAL_DIR)/init.c \
	$(LOCAL_DIR)/debug.c \
	$(LOCAL_DIR)/timer.c \
	$(LOCAL_DIR)/startup_gcc.c \
	$(LOCAL_DIR)/ti_driverlib.c

#	$(LOCAL_DIR)/debug.c \
	$(LOCAL_DIR)/interrupts.c \
	$(LOCAL_DIR)/platform_early.c \
	$(LOCAL_DIR)/platform.c \
	$(LOCAL_DIR)/timer.c \
	$(LOCAL_DIR)/init_clock.c \
	$(LOCAL_DIR)/init_clock_48mhz.c \
	$(LOCAL_DIR)/mux.c \
	$(LOCAL_DIR)/emac_dev.c

# use a two segment memory layout, where all of the read-only sections 
# of the binary reside in rom, and the read/write are in memory. The 
# ROMBASE, MEMBASE, and MEMSIZE make variables are required to be set 
# for the linker script to be generated properly.
#
LINKER_SCRIPT += \
	$(BUILDDIR)/system-twosegment.ld

MODULE_DEPS += \
	lib/cbuf

#include $(LOCAL_DIR)/cmsis/sam3x/rules.mk $(LOCAL_DIR)/drivers/rules.mk

include make/module.mk
