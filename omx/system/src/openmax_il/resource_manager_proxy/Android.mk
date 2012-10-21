LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/ResourceManagerProxy.c

LOCAL_C_INCLUDES += \
        $(TI_OMX_INCLUDES) \
        $(TI_OMX_SYSTEM)/resource_manager_proxy/inc \
        $(TI_OMX_SYSTEM)/perf/inc \
    $(TI_OMX_SYSTEM)/resource_manager/resource_activity_monitor/inc

LOCAL_SHARED_LIBRARIES := \
        libdl \
        libcutils \
        libbridge \
    libRAM \
        libPERF


LOCAL_CFLAGS := $(TI_OMX_CFLAGS)

ifeq ($(ENABLE_RMPM_STUB), 1)
    LOCAL_CFLAGS += -D__ENABLE_RMPM_STUB__
endif

LOCAL_MODULE:= libOMX_ResourceManagerProxy
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

