LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
        src/OMX_JpegDec_Thread.c \
        src/OMX_JpegDec_Utils.c \
        src/OMX_JpegDecoder.c \

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(TI_OMX_IMAGE)/jpeg_dec/inc \

LOCAL_SHARED_LIBRARIES := $(TI_OMX_COMP_SHARED_LIBRARIES)


LOCAL_CFLAGS := $(TI_OMX_CFLAGS) -DOMAP_2430 #-DOMX_DEBUG

LOCAL_MODULE:= libOMX.TI.JPEG.decoder
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


#########################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= tests/JPEGTest.c

LOCAL_C_INCLUDES := $(TI_OMX_COMP_C_INCLUDES) \
        $(TI_OMX_IMAGE)/jpeg_dec/inc \

LOCAL_SHARED_LIBRARIES := libOMX.TI.JPEG.decoder \
        liblog \
        libOMX_Core

LOCAL_CFLAGS := -Wall -fpic -pipe -O0 -DOMX_DEBUG=1

LOCAL_MODULE:= JpegTestCommon
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)


