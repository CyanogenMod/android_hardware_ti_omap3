ifdef HARDWARE_OMX

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

# do not prelink
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
	libskia \
        libutils \
        libcutils \
	$(BOARD_OPENCORE_LIBRARIES)

LOCAL_C_INCLUDES += \
	external/skia/include/core \
	external/skia/include/images \
	$(OMX_VENDOR_INCLUDES)

LOCAL_CFLAGS += -fpic -fstrict-aliasing

LOCAL_SRC_FILES+= \
   SkImageDecoder_libtijpeg.cpp \
   SkImageEncoder_libtijpeg.cpp \
   SkAllocator.cpp \
   SkMemory.cpp \
   
LOCAL_MODULE:= libskiahw

include $(BUILD_SHARED_LIBRARY)

################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES := SkImageDecoderTest.cpp

LOCAL_SHARED_LIBRARIES := \
	libskiahw

LOCAL_MODULE := SkImageDecoderTest

LOCAL_C_INCLUDES += \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/core \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \

include $(BUILD_EXECUTABLE)

################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES := SkImageEncoderTest.cpp

LOCAL_SHARED_LIBRARIES := \
    libskiahw

LOCAL_MODULE := SkImageEncoderTest

LOCAL_C_INCLUDES += \
	external/skia/include/images \
	external/skia/include/core \
	external/skia/include/graphics \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \

include $(BUILD_EXECUTABLE)

################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ../test/libskiahw/SkLibTiJpeg_Test.cpp

LOCAL_SHARED_LIBRARIES := \
	libskiahw

LOCAL_MODULE := SkLibTiJpeg_Test

LOCAL_C_INCLUDES += \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/core \
	hardware/ti/omx/system/src/openmax_il/omx_core/inc \

include $(BUILD_EXECUTABLE)

################################################
endif

