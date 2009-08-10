
LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_SRC_FILES := v4l2_utils.c overlay.cpp
LOCAL_MODULE := overlay.zoom2
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -mabi=aapcs-linux
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_SRC_FILES := v4l2_utils.c v4l2_test.c
LOCAL_MODULE := v4l2_test
include $(BUILD_EXECUTABLE)

