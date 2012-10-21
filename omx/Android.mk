ifeq ($(HARDWARE_OMX),true)
ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_PATH := $(call my-dir)
TI_OMX_TOP := $(LOCAL_PATH)

#build these only for froyo
ifneq ($(filter 2.2%,$(PLATFORM_VERSION)),)
#call to plugin
include $(TI_OMX_TOP)/core_plugin/Android.mk

#call to ti_omx_config_parser
include $(TI_OMX_TOP)/ti_omx_config_parser/Android.mk
endif

#call to ducati domx
include $(TI_OMX_TOP)/ducati/Android.mk
endif

ifeq ($(TARGET_BOARD_PLATFORM),omap3)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

TI_BRIDGE_TOP := hardware/ti/omap3/dspbridge

OMX_DEBUG := 0
RESOURCE_MANAGER_ENABLED := 0
ENABLE_RMPM_STUB := 0
DVFS_ENABLED := 0
PERF_INSTRUMENTATION := 1
PERF_CUSTOMIZABLE := 1
PERF_READER := 1

TI_OMX_CFLAGS := -Wall -fpic -pipe -finline-functions -DSTATIC_TABLE -O0

TI_OMX_TOP := $(LOCAL_PATH)
TI_OMX_SYSTEM := $(TI_OMX_TOP)/system/src/openmax_il
TI_OMX_VIDEO := $(TI_OMX_TOP)/video/src/openmax_il
TI_OMX_AUDIO := $(TI_OMX_TOP)/audio/src/openmax_il
TI_OMX_IMAGE := $(TI_OMX_TOP)/image/src/openmax_il

TI_OMX_INCLUDES := \
        $(TI_OMX_SYSTEM)/omx_core/inc

TI_OMX_COMP_SHARED_LIBRARIES := \
        libdl \
        libcutils \
        liblog

ifeq ($(PERF_INSTRUMENTATION),1)
TI_OMX_COMP_SHARED_LIBRARIES += \
        libPERF
endif

ifeq ($(RESOURCE_MANAGER_ENABLED),1)
TI_OMX_CFLAGS += -DRESOURCE_MANAGER_ENABLED
TI_OMX_COMP_SHARED_LIBRARIES += \
        libOMX_ResourceManagerProxy
endif

ifeq ($(PERF_INSTRUMENTATION),1)
TI_OMX_CFLAGS += -D__PERF_INSTRUMENTATION__
endif

ifeq ($(ENABLE_RMPM_STUB),1)
TI_OMX_CFLAGS += -D__ENABLE_RMPM_STUB__
endif

ifeq ($(DVFS_ENABLED),1)
TI_OMX_CFLAGS += -DDVFS_ENABLED
endif


TI_OMX_COMP_C_INCLUDES := \
        $(TI_OMX_INCLUDES) \
        $(TI_BRIDGE_TOP)/inc \
        $(TI_OMX_SYSTEM)/lcml/inc \
        $(TI_OMX_SYSTEM)/common/inc \
        $(TI_OMX_SYSTEM)/perf/inc \
        $(TI_OMX_SYSTEM)/resource_manager/inc \
        $(TI_OMX_SYSTEM)/resource_manager_proxy/inc \
        $(TI_OMX_SYSTEM)/omx_policy_manager/inc

ifeq ($(PERF_INSTRUMENTATION),1)
include $(TI_OMX_SYSTEM)/perf/Android.mk
endif

ifeq ($(PERF_READER),1)
#TODO: Implement automatic building
#include $(TI_OMX_SYSTEM)/perf/reader/Android.mk
endif

#call to common omx & system components
include $(TI_OMX_SYSTEM)/omx_core/src/Android.mk
include $(TI_OMX_SYSTEM)/lcml/src/Android.mk
#include $(TI_OMX_SYSTEM)/resource_manager/Android.mk
#include $(TI_OMX_SYSTEM)/resource_manager_proxy/Android.mk
#include $(TI_OMX_SYSTEM)/omx_policy_manager/Android.mk

#call to audio
#include $(TI_OMX_TOP)/audio/tests/Android.mk
include $(TI_OMX_AUDIO)/aac_dec/src/Android.mk
include $(TI_OMX_AUDIO)/aac_enc/src/Android.mk
include $(TI_OMX_AUDIO)/aac_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/mp3_dec/src/Android.mk
include $(TI_OMX_AUDIO)/wma_dec/src/Android.mk
include $(TI_OMX_AUDIO)/wma_dec/tests/Android.mk

#call to VoIP/speech
include $(TI_OMX_AUDIO)/nbamr_dec/src/Android.mk
include $(TI_OMX_AUDIO)/nbamr_enc/src/Android.mk
include $(TI_OMX_AUDIO)/nbamr_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/wbamr_dec/src/Android.mk
include $(TI_OMX_AUDIO)/wbamr_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/wbamr_enc/src/Android.mk
include $(TI_OMX_AUDIO)/wbamr_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/g711_dec/src/Android.mk
include $(TI_OMX_AUDIO)/g711_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/g711_enc/src/Android.mk
include $(TI_OMX_AUDIO)/g711_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/g722_dec/src/Android.mk
include $(TI_OMX_AUDIO)/g722_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/g722_enc/src/Android.mk
include $(TI_OMX_AUDIO)/g722_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/g726_dec/src/Android.mk
include $(TI_OMX_AUDIO)/g726_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/g726_enc/src/Android.mk
include $(TI_OMX_AUDIO)/g726_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/g729_dec/src/Android.mk
include $(TI_OMX_AUDIO)/g729_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/g729_enc/src/Android.mk
include $(TI_OMX_AUDIO)/g729_enc/tests/Android.mk
include $(TI_OMX_AUDIO)/ilbc_dec/src/Android.mk
include $(TI_OMX_AUDIO)/ilbc_dec/tests/Android.mk
include $(TI_OMX_AUDIO)/ilbc_enc/src/Android.mk
include $(TI_OMX_AUDIO)/ilbc_enc/tests/Android.mk

#call to video
include $(TI_OMX_VIDEO)/video_decode/Android.mk
#include $(TI_OMX_VIDEO)/video_decode/test/Android.mk
include $(TI_OMX_VIDEO)/video_encode/Android.mk
#include $(TI_OMX_VIDEO)/video_encode/test/Android.mk
#include $(TI_OMX_VIDEO)/prepost_processor/Android.mk

#call ittiam 720p codec
#include $(TI_OMX_AUDIO)/aac_dec_ittiam/Android.mk
#include $(TI_OMX_AUDIO)/aac_enc_ittiam/Android.mk
#include $(TI_OMX_AUDIO)/wma_dec_ittiam/Android.mk
#include $(TI_OMX_VIDEO)/video_decode_ittiam/Android.mk
#include $(TI_OMX_VIDEO)/video_encode_ittiam/Android.mk

#call to image
include $(TI_OMX_IMAGE)/jpeg_enc/Android.mk
#include $(TI_OMX_IMAGE)/jpeg_dec/Android.mk

#build these only for froyo
ifneq ($(filter 2.2%,$(PLATFORM_VERSION)),)
#call to plugin
include $(TI_OMX_TOP)/core_plugin/Android.mk

#call to ti_omx_config_parser
include $(TI_OMX_TOP)/ti_omx_config_parser/Android.mk
endif

endif
endif
