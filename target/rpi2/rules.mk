LOCAL_DIR := $(GET_LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

PLATFORM := bcm28xx

GLOBAL_DEFINES += CRYSTAL=19200000 BCM2836=1
ARCH := arm
ARM_CPU := cortex-a7
HAVE_ARM_TIMER = 1

#include make/module.mk

