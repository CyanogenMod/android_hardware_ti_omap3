LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	TIFlashHWRenderer.cpp \

LOCAL_SHARED_LIBRARIES:= \
        libsurfaceflinger_client \
        libstagefrighthw \
        libstagefright \
        libmedia \
        libbinder \
        libcutils \
        libutils \
        liblog \
        libui \
        libdl \


LOCAL_C_INCLUDES += \
    $(TOP)/system/core/include \
    $(TOP)/frameworks/base/include/ui \
    $(TOP)/frameworks/base/include/surfaceflinger \
    $(TOP)/frameworks/base/include/media/stagefright/openmax \
    $(TOP)/frameworks/base/include/binder \
    $(TOP)/frameworks/base/include/media \
    $(TOP)/external/opencore/android \
    $(TOP)/hardware/ti/omap3/liboverlay \
    $(TOP)/hardware/ti/omap3/libstagefrighthw \
    $(TOP)/hardware/ti/syslink/syslink/api/include \

LOCAL_CFLAGS +=-Wall -fno-short-enums -O0 -g -D___ANDROID___

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
    LOCAL_CFLAGS += -DTARGET_OMAP4
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE:= libflashhwrenderer
LOCAL_MODULE_TAGS:= optional
include $(BUILD_SHARED_LIBRARY)

#############################################################################################

include $(CLEAR_VARS)
ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_CFLAGS := -DTARGET_OMAP4
endif

LOCAL_SHARED_LIBRARIES:= \
        libsurfaceflinger_client \
        libstagefright \
        libbinder \
        libmedia \
        libcutils \
        libutils \
        liblog \
        libui \
        libdl \


LOCAL_C_INCLUDES += \
    $(TOP)/system/core/include \
    $(TOP)/frameworks/base/include/ui \
    $(TOP)/frameworks/base/include/surfaceflinger \
    $(TOP)/frameworks/base/include/media/stagefright/openmax \
    $(TOP)/frameworks/base/include/binder \
    $(TOP)/frameworks/base/include/media \
    $(TOP)/external/opencore/android \
    $(TOP)/hardware/ti/omap3/liboverlay \
    $(TOP)/hardware/ti/omap3/libstagefrighthw \
    $(TOP)/hardware/ti/syslink/syslink/api/include \

LOCAL_SRC_FILES := TIFlashHWRenderer_test.cpp
LOCAL_MODULE := flash_renderer_test
LOCAL_MODULE_TAGS:= optional
include $(BUILD_EXECUTABLE)

