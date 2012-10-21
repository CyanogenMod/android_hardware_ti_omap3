LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        rm_api_test.c \

LOCAL_C_INCLUDES := \
    $(TI_OMX_SYSTEM)/resource_manager/resource_activity_monitor/inc \
    $(TI_OMX_SYSTEM)/common/inc \
    $(TI_OMX_COMP_C_INCLUDES)
        

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES) \
                           libdl \
                           libRAM

LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMX_DEBUG

LOCAL_MODULE:= rm_api_test

include $(BUILD_EXECUTABLE)
