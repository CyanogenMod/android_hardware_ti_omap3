# Copyright 2008 Texas Instruments
#
#Author(s) Mikkel Christensen (mlc@ti.com) and Ulrik Bech Hald (ubh@ti.com)

#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE:= gsm0710muxd
LOCAL_MODULE_TAGS:= optional

LOCAL_SRC_FILES:= \
	src/gsm0710muxd.c \

LOCAL_SHARED_LIBRARIES := \
	libcutils \

	# for asprinf

LOCAL_CFLAGS := -DMUX_ANDROID

LOCAL_LDLIBS := -lpthread


include $(BUILD_EXECUTABLE)




