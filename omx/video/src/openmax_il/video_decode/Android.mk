LOCAL_PATH:= $(call my-dir)

HARDWARE_TI_OMAP3_BASE:=hardware/ti/omap3
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/OMX_VideoDec_Thread.c \
        src/OMX_VideoDec_Utils.c \
        src/OMX_VideoDecoder.c

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(TI_OMX_VIDEO)/video_decode/inc \
        $(HARDWARE_TI_OMAP3_BASE)/ion/ \
        system/core/include/cutils \
        system/core/include/system \
        $(HARDWARE_TI_OMAP3_BASE)/../../libhardware/include \
	    $(HARDWARE_TI_OMAP3_BASE)/hwc/ \  	          
        #hardware/ti/omap3/liboverlay \


ifeq ($(PERF_INSTRUMENTATION),1)
LOCAL_C_INCLUDES += \
        $(TI_OMX_SYSTEM)/perf/inc
endif

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES) \
							libion \
							libhardware

ifeq ($(PERF_INSTRUMENTATION),1)
LOCAL_SHARED_LIBRARIES += \
        libPERF
endif

LOCAL_LDLIBS += \
        -lpthread \

LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DANDROID -DOMAP_2430 -DANDROID_QUIRK_LOCK_BUFFER

LOCAL_MODULE:= libOMX.TI.Video.Decoder
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
