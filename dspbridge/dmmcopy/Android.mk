LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	dmmcopy.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../inc	

LOCAL_SHARED_LIBRARIES := \
	libbridge \
	libion
	
LOCAL_CFLAGS += -Wall -g -O2 -finline-functions -DOMAP_3430

LOCAL_MODULE:= dmmcopy.out
LOCAL_MODULE_TAGS:= optional

include $(BUILD_EXECUTABLE)

