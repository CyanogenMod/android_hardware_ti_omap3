ifdef BOARD_USES_TI_CAMERA_HAL

################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    CameraHal.cpp \
    CameraHal_Utils.cpp \
    MessageQueue.cpp \
    
LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libutils \
    libcutils \

LOCAL_C_INCLUDES += \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay

LOCAL_CFLAGS += -fno-short-enums 

ifdef HARDWARE_OMX

LOCAL_SRC_FILES += \
    scale.c \
    JpegEncoder.cpp \

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/dspbridge/api/inc \
	hardware/ti/omx/system/src/openmax_il/lcml/inc \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \
	hardware/ti/omx/system/src/openmax_il/common/inc	
	
LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX

LOCAL_SHARED_LIBRARIES += \
    libbridge \
    libLCML \
    libOMX_Core \

endif


ifdef FW3A

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/mm_isp/ipp/inc \
	hardware/ti/omap3/mm_isp/capl/inc \
	hardware/ti/omap3/fw3A/include \
	hardware/ti/omap3/arcsoft \
	hardware/ti/omap3/arcsoft/include \
	hardware/ti/omap3/arcsoft/arc_redeye/include \
	hardware/ti/omap3/arcsoft/arc_facetracking/include \
	hardware/ti/omap3/arcsoft/arc_antishaking/include \
	hardware/ti/omap3/mms_ipp_new/ARM11/inc

LOCAL_SHARED_LIBRARIES += \
    libdl \
    libcapl \
    libImagePipeline \
#    libarcsoft \
#    libpip_omap

LOCAL_CFLAGS += -O0 -g3 -DIPP_LINUX -D___ANDROID___ -DFW3A -DICAP -DIMAGE_PROCESSING_PIPELINE #_MMS -DCAMERA_ALGO

endif

LOCAL_MODULE:= libcamera

include $(BUILD_SHARED_LIBRARY)

################################################

ifdef HARDWARE_OMX

include $(CLEAR_VARS)

LOCAL_SRC_FILES := JpegEncoderTest.cpp

LOCAL_C_INCLUDES := hardware/ti/omx/system/src/openmax_il/omx_core/inc

LOCAL_SHARED_LIBRARIES := libcamera

LOCAL_MODULE := JpegEncoderTest

include $(BUILD_EXECUTABLE)

endif

################################################

endif

