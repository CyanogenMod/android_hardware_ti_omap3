ifdef BOARD_USES_TI_CAMERA_HAL

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	camera_test.cpp

LOCAL_SHARED_LIBRARIES:= \
	libdl \
	libui \
	libutils \
	libcutils \
	libbinder \
	libmedia \

LOCAL_C_INCLUDES += \
	frameworks/base/include/ui \
	frameworks/base/include/media \
	$(PV_INCLUDES)

LOCAL_MODULE:= camera_test

LOCAL_CFLAGS += -Wall -fno-short-enums -O0 -g -D___ANDROID___

include $(BUILD_EXECUTABLE)

endif # BOARD_USES_TI_CAMERA_HAL

