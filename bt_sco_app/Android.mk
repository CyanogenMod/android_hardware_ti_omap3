ifeq ($(BOARD_HAVE_BLUETOOTH),true)
LOCAL_PATH:= $(call my-dir)

#
# bt_sco_app 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	external/bluetooth/bluez/lib

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	bt_sco_app.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth  libcutils
#LOCAL_STATIC_LIBRARIES := \
#	libbluez-common-static
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=bt_sco_app

include $(BUILD_EXECUTABLE)

endif
