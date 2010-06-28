ifeq ($(BOARD_HAVE_BLUETOOTH),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Module name should match apk name to be installed.
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := BlueFTP
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
LOCAL_PACKAGE_NAME := BlueFTP
LOCAL_MODULE_CLASS := APPS
LOCAL_CERTIFICATE := shared
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
include $(BUILD_PREBUILT)
endif
