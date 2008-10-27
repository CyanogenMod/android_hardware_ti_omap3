LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	strmcopy.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc	

LOCAL_SHARED_LIBRARIES := \
	libbridge
	
LOCAL_CFLAGS += -Wall -g -O2 -finline-functions -DOMAP_3430

LOCAL_MODULE:= strmcopy.out

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	strmcopy_dyn.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc	

LOCAL_SHARED_LIBRARIES := \
	libbridge
	
LOCAL_CFLAGS += -Wall -g -O2 -finline-functions -DOMAP_3430

LOCAL_MODULE:= strmcopy_dyn.out

include $(BUILD_EXECUTABLE)
