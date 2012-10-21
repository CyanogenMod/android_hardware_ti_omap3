LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/PolicyManager.c

LOCAL_C_INCLUDES += \
        $(TI_OMX_INCLUDES) \
        $(TI_BRIDGE_TOP)/api/inc \
        $(TI_OMX_SYSTEM)/omx_policy_manager/inc \
        $(TI_OMX_SYSTEM)/resource_manager_proxy/inc \
        $(TI_OMX_SYSTEM)/perf/inc

LOCAL_SHARED_LIBRARIES := \
        libdl \
        libcutils \
        libbridge

LOCAL_LDLIBS += \
        -lpthread \
        -ldl \
        -lsdl

LOCAL_CFLAGS := $(TI_OMX_CFLAGS)

LOCAL_MODULE:= OMXPolicyManager
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

