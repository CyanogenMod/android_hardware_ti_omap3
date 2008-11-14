LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libaudio

LOCAL_SHARED_LIBRARIES := \
       libasound \
       libcutils \
       libutils \
       libmedia \
       libhardware

LOCAL_STATIC_LIBRARIES += \
	libaudiointerface

LOCAL_SRC_FILES += \
       AudioStreamInOmap.cpp \
       AudioStreamOutOmap.cpp \
       AudioHardwareOmap.cpp

LOCAL_CFLAGS +=

# FIXME
# need to update this once alsa moves to the Android core
LOCAL_C_INCLUDES += \
       $(LOCAL_PATH)/../alsa-lib-1.0.13/include

include $(BUILD_SHARED_LIBRARY)

