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
    $(LOCAL_PATH)/../../../tiler/memmgr \
    $(LOCAL_PATH)/../../../../../external/libxml2/include \


LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libmemmgr \

LOCAL_STATIC_LIBRARIES:= \
    libxml2 \


LOCAL_C_INCLUDES += \
        kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \


LOCAL_CFLAGS += -fno-short-enums

LOCAL_MODULE:= libcamera

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

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils \
    libtiutils \
    libcamera \


LOCAL_C_INCLUDES += \
        kernel/android-2.6.29/include \
	frameworks/base/include/ui \
	hardware/ti/omap3/liboverlay \
	hardware/ti/omap3/libtiutils \
	frameworks/base/include/utils \
	$(LOCAL_PATH)/../../../../../external/libxml2/include \



LOCAL_CFLAGS += -fno-short-enums

LOCAL_MODULE:= libfakecameraadapter

include $(BUILD_SHARED_LIBRARY)

################################################

#Insert OMX Camera Adapter in the future

endif
endif

