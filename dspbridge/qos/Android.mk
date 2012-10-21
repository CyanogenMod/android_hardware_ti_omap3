LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	QOSComponent.c \
	QOSResource.c \
	QOSRegistry.c \
	qosti.c \

LOCAL_SHARED_LIBRARIES := \
	libbridge

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../inc
	
LOCAL_CFLAGS += -MD -pipe -fomit-frame-pointer -Wall  -Wno-trigraphs -Werror-implicit-function-declaration  -fno-strict-aliasing -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -msoft-float -Uarm -DMODULE -D__LINUX_ARM_ARCH__=7  -fno-common -DLINUX -DOMAP_3430 -fpic

LOCAL_MODULE:= libqos
LOCAL_MODULE_TAGS:= optional

include $(BUILD_STATIC_LIBRARY)

