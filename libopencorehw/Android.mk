LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Set up the OpenCore variables.
#include external/opencore/Config.mk
LOCAL_C_INCLUDES := $(PV_INCLUDES)

LOCAL_SRC_FILES := \
	android_surface_output_omap34xx.cpp

LOCAL_CFLAGS := $(PV_CFLAGS)

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libui \
    libhardware\
    libandroid_runtime \
    libmedia \
    libsgl \
    libopencore_common \
    libicuuc \
    libopencore_player

# do not prelink
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libopencorehw

LOCAL_LDLIBS += 

include $(BUILD_SHARED_LIBRARY)

