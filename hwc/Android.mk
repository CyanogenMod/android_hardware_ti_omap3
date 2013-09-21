# HWC under heavy development and should not be included in builds for now
LOCAL_PATH := $(call my-dir)

# HAL module implementation, not prelinked and stored in
# hw/<HWCOMPOSE_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/../vendor/lib/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libutils libhardware libhardware_legacy
LOCAL_SRC_FILES := hwc.c

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := hwcomposer.omap3
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DLOG_TAG=\"ti_hwc\"
# LOG_NDEBUG=0 means verbose logging enabled
# LOCAL_CFLAGS += -DLOG_NDEBUG=0

ifneq ($(TARGET_SCREEN_WIDTH),)
	LOCAL_CFLAGS += -DSCREEN_WIDTH=$(TARGET_SCREEN_WIDTH)
endif

ifneq ($(TARGET_SCREEN_HEIGHT),)
	LOCAL_CFLAGS += -DSCREEN_HEIGHT=$(TARGET_SCREEN_HEIGHT)
endif

include $(BUILD_SHARED_LIBRARY)
