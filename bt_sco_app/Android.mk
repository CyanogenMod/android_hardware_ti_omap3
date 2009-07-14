LOCAL_PATH:= $(call my-dir)

#
# bt_sco_app 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	bt_sco_app.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth  libcutils
LOCAL_STATIC_LIBRARIES := \
        libbluez-utils-common-static
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=bt_sco_app

include $(BUILD_EXECUTABLE)

