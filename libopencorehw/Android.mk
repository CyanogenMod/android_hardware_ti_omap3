ifeq (0,1)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Set up the OpenCore variables.
#include external/opencore/Config.mk
LOCAL_C_INCLUDES := $(PV_INCLUDES)

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_CFLAGS := -DTARGET_OMAP4
endif

LOCAL_SRC_FILES := \
	android_surface_output_omap34xx.cpp \
	buffer_alloc_omap34xx.cpp \

LOCAL_C_INCLUDES := $(PV_INCLUDES) \
        hardware/ti/omap3/liboverlay

LOCAL_CFLAGS := $(PV_CFLAGS)

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libui \
    libhardware\
    libandroid_runtime \
    libmedia \
    libopencore_common \
    libicuuc \
    libopencore_player

#    libsgl \
# do not prelink
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libopencorehw

include $(BUILD_SHARED_LIBRARY)
endif
