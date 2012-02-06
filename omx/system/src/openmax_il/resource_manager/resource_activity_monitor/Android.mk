LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/Resource_Activity_Monitor.c

LOCAL_C_INCLUDES += \
        $(TI_OMX_INCLUDES) \
        $(TI_BRIDGE_TOP)/inc \
        $(TI_OMX_SYSTEM)/resource_manager/resource_activity_monitor/inc

LOCAL_SHARED_LIBRARIES := \
        libdl \
        libcutils


LOCAL_CFLAGS := $(TI_OMX_CFLAGS)

LOCAL_MODULE:= libRAM
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

