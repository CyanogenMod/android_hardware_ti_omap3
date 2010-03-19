LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	instutility.c \

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc

LOCAL_SHARED_LIBRARIES := \
	libbridge

LOCAL_CFLAGS += -Wall -g -O0 -finline-functions

LOCAL_MODULE:= instutility.out

include $(BUILD_EXECUTABLE)

