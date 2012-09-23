LOCAL_PATH:= $(call my-dir)

HARDWARE_TI_OMAP3_BASE:=hardware/ti/omap3

include $(CLEAR_VARS)

TI_BRIDGE_TOP := hardware/ti/omap3/dspbridge
TI_OMX_SYSTEM := hardware/ti/omap3/omx/system/src/openmax_il
TI_OMX_INCLUDES := \
        $(TI_OMX_SYSTEM)/omx_core/inc
TI_OMX_COMP_C_INCLUDES := \
        $(TI_OMX_INCLUDES) \
        $(TI_BRIDGE_TOP)/inc \
        $(TI_OMX_SYSTEM)/lcml/inc \
        $(TI_OMX_SYSTEM)/common/inc \
        $(TI_OMX_SYSTEM)/perf/inc \
        $(TI_OMX_SYSTEM)/resource_manager/inc \
        $(TI_OMX_SYSTEM)/resource_manager_proxy/inc \
        $(TI_OMX_SYSTEM)/omx_policy_manager/inc

TI_OMX_COMP_SHARED_LIBRARIES := \
        libdl \
        libcutils \
        liblog

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/OMX_VideoEnc_Thread.c \
        src/OMX_VideoEnc_Utils.c \
        src/OMX_VideoEncoder.c

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(LOCAL_PATH)/inc \
        $(TI_OMX_VIDEO)/video_encode/inc \
        $(HARDWARE_TI_OMAP3_BASE)/camera/inc \
        frameworks/av/include/media/stagefright \
        frameworks/native/include/media/hardware \

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES) libPERF


LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMAP_2430

LOCAL_MODULE:= libOMX.TI.Video.encoder
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
