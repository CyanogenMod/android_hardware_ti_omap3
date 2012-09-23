ifeq (1,2) # DISABLED. API NEEDS UPDATING TO JB
ifeq ($(TARGET_BOARD_PLATFORM),omap3)

LOCAL_PATH:= $(call my-dir)

OMAP3_CAMERA_HAL_SRC := \
	CameraHal_Module.cpp \
	CameraHal.cpp \
	CameraHalUtilClasses.cpp \
	AppCallbackNotifier.cpp \
	ANativeWindowDisplayAdapter.cpp \
	CameraProperties.cpp \
	MemoryManager.cpp \
	Encoder_libjpeg.cpp \
	SensorListener.cpp  \
	NV12_resize.c \
	CameraHal_Utils.cpp

OMAP3_CAMERA_COMMON_SRC:= \
	CameraParameters.cpp \
	TICameraParameters.cpp \
	CameraHalCommon.cpp

OMAP3_CAMERA_USB_SRC:= \
	BaseCameraAdapter.cpp \
	V4LCameraAdapter/V4LCameraAdapter.cpp

#
# USB Camera Adapter
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	$(OMAP3_CAMERA_HAL_SRC) \
	$(OMAP3_CAMERA_USB_SRC) \
	$(OMAP3_CAMERA_COMMON_SRC)

LOCAL_C_INCLUDES := hardware/ti/omap3/dspbridge/inc \
    hardware/ti/omap3/omx/system/src/openmax_il/lcml/inc \
    $(HARDWARE_TI_OMAP3_BASE)/omx/system/src/openmax_il/omx_core/inc \
    hardware/ti/omap3/omx/system/src/openmax_il/common/inc \
    hardware/ti/omap3/omx/image/src/openmax_il/jpeg_enc/inc \
    hardware/ti/omap3/libexif \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/../hwc \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/inc/V4LCameraAdapter \
    $(LOCAL_PATH)/../libtiutils \
    hardware/ti/omap3/ion \
    frameworks/base/include/ui \
    frameworks/native/include/utils \
    frameworks/base/include \
    frameworks/base/include/camera \
    frameworks/base/include/media/stagefright \
    frameworks/base/include/media/stagefright/openmax \
    external/jhead \
    external/jpeg

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libcamera_client \
    libion \
    libjpeg \
    libexif \
    libgui \
    libdl

LOCAL_CFLAGS := -fno-short-enums -DCOPY_IMAGE_BUFFER

ifdef HARDWARE_OMX

LOCAL_SRC_FILES += \
    scale.c \
    JpegEncoder.cpp \
    JpegEncoderEXIF.cpp \

LOCAL_CFLAGS += -O0 -g3 -fpic -fstrict-aliasing -DIPP_LINUX -D___ANDROID___ -DHARDWARE_OMX

LOCAL_SHARED_LIBRARIES += \
    libbridge \
    libLCML \
    libOMX_Core

LOCAL_STATIC_LIBRARIES := \
        libexifgnu

endif

ifdef FW3A

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/fw3A/include/fw/api/linux

LOCAL_CFLAGS += -DFW3A

LOCAL_SHARED_LIBRARIES += \
	libcapl \
	libicamera \
	libicapture \
	libImagePipeline

endif

ifdef ICAP

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/mm_isp/capl/inc/

LOCAL_CFLAGS += -DICAP

endif

ifdef IMAGE_PROCESSING_PIPELINE

LOCAL_C_INCLUDES += \
	hardware/ti/omap3/mm_isp/ipp/inc/

LOCAL_CFLAGS += -DIMAGE_PROCESSING_PIPELINE

endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE:= camera.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS:= optional

include $(BUILD_HEAPTRACKED_SHARED_LIBRARY)
endif
endif
