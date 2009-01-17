LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    CameraHal.cpp

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libutils \
    libcutils \
    libsgl

LOCAL_C_INCLUDES += \
	frameworks/base/include/ui

ifdef HARDWARE_OMX

LOCAL_C_INCLUDES += \
	external/skia/libsgl/images \
	external/skia/include/corecg \
	external/skia/include/graphics \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \
	hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc \
	frameworks/base/include/ui

endif

LOCAL_MODULE:= libcamera

LOCAL_CFLAGS += -fno-short-enums

include $(BUILD_SHARED_LIBRARY)

