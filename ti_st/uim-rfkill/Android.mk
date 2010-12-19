LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# UIM Application
#

LOCAL_C_INCLUDES:= uim.h \
	external/bluetooth/bluez/

LOCAL_SRC_FILES:= \
	uim.c
LOCAL_CFLAGS:= -g -c -W -Wall -O2 -D_POSIX_SOURCE
LOCAL_SHARED_LIBRARIES:= libnetutils
LOCAL_MODULE:=uim-rfkill
LOCAL_MODULE_TAGS:= optional
include $(BUILD_EXECUTABLE)


