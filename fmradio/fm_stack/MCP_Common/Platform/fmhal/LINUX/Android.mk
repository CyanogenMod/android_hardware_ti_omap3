ifeq ($(TARGET_ARCH),arm)

BTIPS_DEBUG?=0
BTIPS_TARGET_PLATFORM?=zoom2

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -DANDROID -W -Wall 

ifeq ($(BTIPS_DEBUG),1)
    LOCAL_CFLAGS+= -g -O0
    LOCAL_LDFLAGS+= -g
else
    LOCAL_CFLAGS+= -O2 -DEBTIPS_RELEASE
endif

LOCAL_SRC_FILES:= \
                common/os/fmc_os.c 

LOCAL_C_INCLUDES := \
                $(LOCAL_PATH)/common/inc\
                $(LOCAL_PATH)/android_$(BTIPS_TARGET_PLATFORM)/inc\
                $(LOCAL_PATH)/../inc \
                $(LOCAL_PATH)/../inc/int \
                $(LOCAL_PATH)/../../os/LINUX/common/inc/ \
                $(LOCAL_PATH)/../../os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc/ \
                $(LOCAL_PATH)/../../inc \
                $(LOCAL_PATH)/../../../tran \
                $(LOCAL_PATH)/../../../inc \

LOCAL_MODULE:=libfmhal
LOCAL_MODULE_TAGS:=optional

include $(BUILD_STATIC_LIBRARY)

endif
