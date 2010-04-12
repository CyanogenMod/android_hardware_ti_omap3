LOCAL_PATH := $(call my-dir)
ALSA_PATH := external/alsa-lib/

include $(CLEAR_VARS)

ifeq ($(BUILD_FM_RADIO), true)

#
## Kerenl FM Driver Test Application
#
#
LOCAL_C_INCLUDES:=\
        external/bluetooth/bluez/include \
        $(ALSA_PATH)/include

LOCAL_CFLAGS:= -g -c -W -Wall -O2 -D_POSIX_SOURCE

LOCAL_SRC_FILES:= \
	kfmapp.c

LOCAL_SHARED_LIBRARIES := \
        libaudio

LOCAL_STATIC_LIBRARIES :=

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:=kfmapp

include $(BUILD_EXECUTABLE)

endif # BUILD_FMAPP_

