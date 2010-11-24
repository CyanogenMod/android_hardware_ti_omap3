LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    main.cpp \

LOCAL_SHARED_LIBRARIES:= \
	libaudio \
	libhardware_legacy

LOCAL_C_INCLUDES += \
	hardware/alsa_sound \
	external/alsa-lib/include

LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_MODULE:= audiotest

include $(BUILD_EXECUTABLE)
