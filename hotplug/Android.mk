ifeq ($(BUILD_HOTPLUG), true)
#
# hotplug Application
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
	hotplug.c
LOCAL_SHARED_LIBRARIES:= libcutils
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
# needs to be in same place  as CONFIG_UEVENT_HELPER_PATH from
# kernel .config
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=hotplug
include $(BUILD_EXECUTABLE)

endif # BUILD_HOTPLUG
