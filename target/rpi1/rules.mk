LOCAL_DIR := $(GET_LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

PLATFORM := bcm28xx
ARCH := arm
ARM_CPU := arm1176jzf-s
HAVE_ARM_TIMER = 0

GLOBAL_DEFINES += CRYSTAL=19200000 BCM2835=1

#include make/module.mk

ENABLE_THUMB := false
ARM_WITHOUT_VFP_NEON := true
