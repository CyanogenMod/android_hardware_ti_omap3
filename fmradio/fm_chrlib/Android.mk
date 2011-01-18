
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#
# FM Adaptation
#

LOCAL_C_INCLUDES:= system/bluetooth/bluez-clean-headers

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_SRC_FILES:= fm_chrlib.c

LOCAL_MODULE_TAGS := eng

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE:=libfmchr

include $(BUILD_SHARED_LIBRARY)

