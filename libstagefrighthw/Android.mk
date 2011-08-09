LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_CFLAGS := -DTARGET_OMAP4
endif

LOCAL_SRC_FILES := \
    stagefright_overlay_output.cpp \
    TIHardwareRenderer.cpp \
    TIOMXPlugin.cpp \
    TIOMXCodec.cpp

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/base/include/media/stagefright/openmax \
        $(TOP)/hardware/ti/omap3/liboverlay \
	$(TOP)/hardware/ti/omx/ducati/domx/system/omx_core/inc
LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libui                   \
        libdl					\
        libsurfaceflinger_client \
        libstagefright \
        libmedia \
        liblog \

LOCAL_MODULE := libstagefrighthw

include $(BUILD_SHARED_LIBRARY)
