
LOCAL_PATH:= $(call my-dir)

ifdef HARDWARE_OMX
################################################

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := libskia

LOCAL_WHOLE_STATIC_LIBRARIES := libc_common

LOCAL_SRC_FILES := SkLibTiJpeg_Test.cpp

LOCAL_MODULE := SkLibTiJpeg_Test
LOCAL_MODULE_TAGS:= optional

LOCAL_C_INCLUDES += \
    external/skia/include/images \
    external/skia/include/core \
    bionic/libc/bionic

ifeq ($(TARGET_BOARD_PLATFORM),omap3)
LOCAL_C_INCLUDES += \
    hardware/ti/omap3/libskiahw-omap3 \
    hardware/ti/omx/system/src/openmax_il/omx_core/inc

LOCAL_SHARED_LIBRARIES := libskia \
                          libskiahw \
                          libcutils

endif

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_C_INCLUDES += \
    hardware/ti/omap3/libskiahw-omap4 \
    hardware/ti/omx/ducati/domx/system/omx_core/inc

LOCAL_SHARED_LIBRARIES := libskia \
                          libskiahwdec

LOCAL_CFLAGS += -DTARGET_OMAP4

endif

include $(BUILD_EXECUTABLE)

################################################
endif


