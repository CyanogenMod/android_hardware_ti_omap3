
################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
    MessageQueue.cpp \
    Semaphore.cpp \
    ErrorUtils.cpp \


    
LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \

LOCAL_C_INCLUDES += \
        kernel/android-2.6.29/include \
	frameworks/base/include/utils \
	bionic/libc/include \
	hardware/ti/omx/ducati/domx/system/omx_core/inc \
	hardware/ti/omx/ducati/domx/system/mm_osal/inc \
	

LOCAL_CFLAGS += -fno-short-enums 

LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX


LOCAL_MODULE:= libtiutils
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)



