ifdef BOARD_USES_TI_CAMERA_HAL

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    CameraHal.cpp

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libutils \
    libcutils \
    libskiahw \

LOCAL_C_INCLUDES += \
	frameworks/base/include/ui \
	frameworks/base/camera/libcameraservice

LOCAL_CFLAGS += -fno-short-enums 

ifdef HARDWARE_OMX

LOCAL_C_INCLUDES += \
	external/skia/include/images \
	external/skia/include/core \
	hardware/ti/omap3/libskiahw \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \

LOCAL_CFLAGS += -DHARDWARE_OMX

LOCAL_SHARED_LIBRARIES += libskiahw

endif

LOCAL_MODULE:= libcamera

include $(BUILD_SHARED_LIBRARY)

endif
