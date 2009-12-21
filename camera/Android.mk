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
    libbinder \
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
	hardware/ti/omx/system/src/openmax_il/common/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager_proxy/inc \
	hardware/ti/omx/system/src/openmax_il/resource_manager/resource_activity_monitor/inc
	
LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX

LOCAL_SHARED_LIBRARIES += \
    libbridge \
    libLCML \
    libOMX_Core \
    libOMX_ResourceManagerProxy

endif


ifdef FW3A

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/mm_isp/ipp/inc \
	hardware/ti/omap3/mm_isp/capl/inc \
	hardware/ti/omap3/fw3A/include

LOCAL_SHARED_LIBRARIES += \
    libdl \
    libcapl \
    libImagePipeline

LOCAL_CFLAGS += -O0 -g3 -DIPP_LINUX -D___ANDROID___ -DFW3A -DICAP -DIMAGE_PROCESSING_PIPELINE

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

