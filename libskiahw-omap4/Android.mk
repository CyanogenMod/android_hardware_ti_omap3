ifdef HARDWARE_OMX
ifeq ($(TARGET_BOARD_PLATFORM),omap4)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

# do not prelink
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
        libskia \
        libutils \
        libcutils \
        libOMX_Core \
        libmemmgr \
        libOMX_CoreOsal \
        $(BOARD_OPENCORE_LIBRARIES)


LOCAL_C_INCLUDES += \
        external/skia/include/core \
        external/skia/include/images \
	external/skia/src/images \
        hardware/ti/omx/ducati/domx/system/omx_core/inc \
        hardware/ti/tiler/memmgr \
        hardware/ti/omx/ducati/domx/system/mm_osal/inc \
        $(OMX_VENDOR_INCLUDES)

LOCAL_CFLAGS += -fpic -fstrict-aliasing

LOCAL_SRC_FILES+= \
        SkImageDecoder_libtijpeg.cpp \
        SkAllocator.cpp \
        SkMemory.cpp \

LOCAL_MODULE:= libskiahwdec

include $(BUILD_SHARED_LIBRARY)

################################################

endif
endif

