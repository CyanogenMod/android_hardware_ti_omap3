
LOCAL_PATH:= $(call my-dir)

ifdef HARDWARE_OMX
################################################

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := libskia

LOCAL_WHOLE_STATIC_LIBRARIES := libc_common

LOCAL_SRC_FILES := SkLibTiJpeg_Test.cpp

LOCAL_MODULE := SkLibTiJpeg_Test

LOCAL_C_INCLUDES += \
	external/skia/include/images \
	external/skia/include/core \
	bionic/libc/bionic

include $(BUILD_EXECUTABLE)

################################################
endif


