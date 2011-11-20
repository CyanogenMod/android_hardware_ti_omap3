LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := 01_Vendor_ti_omx.cfg
LOCAL_MODULE := 01_Vendor_ti_omx.cfg
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)

include $(TI_OMX_TOP)/core_plugin/omx_core_plugin/Android.mk
