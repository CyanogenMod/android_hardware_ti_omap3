LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

# do not prelink
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
	libcorecg \
	libsgl \
	$(BOARD_OPENCORE_LIBRARIES)

LOCAL_C_INCLUDES += \
	external/skia/include/core \
	external/skia/include/images \
	$(OMX_VENDOR_INCLUDES)

LOCAL_CFLAGS += -fpic -fstrict-aliasing

LOCAL_SRC_FILES+= \
   SkImageDecoder_libtijpeg.cpp \
   SkImageEncoder_libtijpeg.cpp 
   
LOCAL_MODULE:= libskiahw

include $(BUILD_SHARED_LIBRARY)

