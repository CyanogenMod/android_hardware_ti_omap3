ifdef BOARD_USES_TI_CAMERA_HAL

ifeq ($(TARGET_BOARD_PLATFORM),omap4)

################################################

#Camera HAL

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
    CameraHal.cpp \
    CameraHalUtilClasses.cpp \
    AppCallbackNotifier.cpp \
    MemoryManager.cpp	\
    OverlayDisplayAdapter.cpp \
    CameraProperties.cpp \
    TICameraParameters.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../inc \
    $(LOCAL_PATH)/../../../tiler \
    $(LOCAL_PATH)/../../../../../external/libxml2/include \
    external/icu4c/common \


LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libtimemmgr \
    libicuuc \
    libcamera_client \

LOCAL_STATIC_LIBRARIES:= \
    libxml2 \


LOCAL_C_INCLUDES += \
        kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \

LOCAL_CFLAGS += -fno-short-enums -DCOPY_IMAGE_BUFFER -DTARGET_OMAP4 -mfpu=neon

LOCAL_MODULE:= libcamera
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)


################################################

#Fake Camera Adapter

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	BaseCameraAdapter.cpp \
	FakeCameraAdapter.cpp \

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../inc \
    external/icu4c/common \

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libcamera \
    libicuuc \
    libcamera_client \

LOCAL_C_INCLUDES += \
        kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \
	$(LOCAL_PATH)/../../../../../external/libxml2/include \

LOCAL_CFLAGS += -fno-short-enums -DTARGET_OMAP4

LOCAL_MODULE:= libfakecameraadapter
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)

################################################

#OMX Camera Adapter

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	BaseCameraAdapter.cpp \
	OMXCameraAdapter/OMXCap.cpp \
	OMXCameraAdapter/OMXCameraAdapter.cpp \


LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../inc/ \
    $(LOCAL_PATH)/../inc/OMXCameraAdapter \
    external/icu4c/common \

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libOMX_CoreOsal \
    libOMX_Core \
    libsysmgr \
    librcm \
    libipc \
    libcamera \
    libicuuc \
    libcamera_client \
    libomx_rpc \

LOCAL_C_INCLUDES += \
    kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \
	hardware/ti/omx/ducati/domx/system/omx_core/inc \
	hardware/ti/omx/ducati/domx/system/mm_osal/inc \
	$(LOCAL_PATH)/../../../../../external/libxml2/include \



LOCAL_CFLAGS += -fno-short-enums -DTARGET_OMAP4

LOCAL_MODULE:= libomxcameraadapter
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)

################################################

#USB Camera Adapter

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	BaseCameraAdapter.cpp \
	V4LCameraAdapter/V4LCameraAdapter.cpp \


LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../inc/ \
    $(LOCAL_PATH)/../inc/V4LCameraAdapter \
    external/icu4c/common \

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libsysmgr \
    librcm \
    libipc \
    libcamera \
    libicuuc \
    libcamera_client \
    libomx_rpc \

LOCAL_C_INCLUDES += \
    kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \
	hardware/ti/omx/ducati/domx/system/omx_core/inc \
	hardware/ti/omx/ducati/domx/system/mm_osal/inc \
	$(LOCAL_PATH)/../../../../../external/libxml2/include \



LOCAL_CFLAGS += -fno-short-enums -DTARGET_OMAP4

LOCAL_MODULE:= libusbcameraadapter
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
endif

