# hardware/ti/omap3/modules/audio_modem/Android.mk
#
# Copyright 2009-2010 Texas Instruments
#

ifeq ($(strip $(BOARD_USES_TI_OMAP_MODEM_AUDIO)),true)

  LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_ARM_MODE := arm
  LOCAL_CFLAGS := -D_POSIX_SOURCE

  LOCAL_C_INCLUDES := hardware/ti/omap3/modules/alsa

  LOCAL_SRC_FILES := audio_modem_generic.cpp

  LOCAL_PRELINK_MODULE := false
  LOCAL_MODULE := libaudiomodemgeneric
  LOCAL_MODULE_TAGS:= optional

  LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libc

  include $(BUILD_SHARED_LIBRARY)

endif
