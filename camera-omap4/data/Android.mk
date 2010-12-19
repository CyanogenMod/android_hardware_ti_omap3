LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS:= optional

$(call add-prebuilt-files, ETC, TICameraCameraProperties.xml)


