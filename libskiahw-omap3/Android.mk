ifeq ($(TARGET_BOARD_PLATFORM),omap3)

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
   SkImageUtility.cpp \
   SkImageDecoder_libtijpeg.cpp \
   SkImageDecoder_libtijpeg_entry.cpp \
   SkImageEncoder_libtijpeg.cpp \
   SkImageEncoder_libtijpeg_entry.cpp \
   SkAllocator.cpp \
   SkMemory.cpp \
   
LOCAL_MODULE:= libskiahw

include $(BUILD_SHARED_LIBRARY)

endif
endif

