LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	scale.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc	

LOCAL_SHARED_LIBRARIES := \
	libbridge
	
LOCAL_CFLAGS += -Wall -g -O2 -finline-functions -DOMAP_3430

LOCAL_MODULE:= scale.out

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	scale_dyn.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc	

LOCAL_SHARED_LIBRARIES := \
	libbridge
	
LOCAL_CFLAGS += -Wall -g -O2 -finline-functions -DOMAP_3430

LOCAL_MODULE:= scale_dyn.out

include $(BUILD_EXECUTABLE)
