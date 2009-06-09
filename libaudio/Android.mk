
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libaudio

LOCAL_WHOLE_STATIC_LIBRARIES := libasound

LOCAL_STATIC_LIBRARIES := libaudiointerface

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libmedia \
	libhardware_legacy \
	libdl \
	libc

LOCAL_SRC_FILES += \
       AudioStreamInOmap.cpp \
       AudioStreamOutOmap.cpp \
       AudioHardwareOmap.cpp

LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_C_INCLUDES += \
	external/alsa-lib/include

include $(BUILD_SHARED_LIBRARY)

endif
