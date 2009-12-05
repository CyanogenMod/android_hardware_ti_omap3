# hardware/ti/omap3/modules/alsa/Android.mk
#
# Copyright 2009 Texas Instruments
#

# This is the OMAP3 ALSA module for OMAP3

ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

  LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

  LOCAL_C_INCLUDES += hardware/alsa_sound external/alsa-lib/include

  LOCAL_SRC_FILES:= alsa_omap3.cpp

  LOCAL_SHARED_LIBRARIES := \
	libaudio \
  	libasound \
  	liblog

  LOCAL_MODULE:= alsa.$(TARGET_PRODUCT)

  include $(BUILD_SHARED_LIBRARY)

endif
