LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/OMX_VPP.c \
        src/OMX_VPP_Utils.c \
        src/OMX_VPP_CompThread.c \
        src/OMX_VPP_ImgConv.c \

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(TI_OMX_VIDEO)/prepost_processor/inc \

ifeq ($(PERF_INSTRUMENTATION),1)
LOCAL_C_INCLUDES += \
        $(TI_OMX_SYSTEM)/perf/inc
endif

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)

ifeq ($(PERF_INSTRUMENTATION),1)
LOCAL_SHARED_LIBRARIES += \
        libPERF
endif

LOCAL_LDLIBS += \
        -lpthread \

LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DANDROID -DOMAP_2430 -g

LOCAL_MODULE:= libOMX.TI.VPP
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

#########################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= tests/VPPTest.c

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(TI_OMX_VIDEO)/prepost_processor/inc \

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES) \
        libOMX_Core

LOCAL_CFLAGS := $(TI_OMX_CFLAGS)

LOCAL_MODULE:= VPPTest_common
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)


