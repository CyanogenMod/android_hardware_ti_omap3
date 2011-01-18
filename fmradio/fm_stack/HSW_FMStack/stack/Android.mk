ifeq ($(TARGET_ARCH),arm)

BTIPS_DEBUG?=0
BTIPS_TARGET_PLATFORM?=zoom2
FM_MCP_STK?=1

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(PLATFORM_VERSION),1.6)
    BTIPS_SDK_VER=1.6
endif	
ifeq ($(PLATFORM_VERSION),2.2)
    BTIPS_SDK_VER=2.2
endif	
BTIPS_SDK_VER?=2.0

LOCAL_CFLAGS+= -DANDROID -W -Wall 

ifeq ($(BTIPS_DEBUG),1)
    LOCAL_CFLAGS+= -g -O0
    LOCAL_LDFLAGS+= -g
else
    LOCAL_CFLAGS+= -O2 -DEBTIPS_RELEASE -DMCP_STK_ENABLE -DBLUEZ_SOLUTION
endif

LOCAL_SRC_FILES:= \
                common/fmc_debug.c \
                common/fmc_utils.c \
                common/fmc_common.c \
                common/fmc_pool.c \
                common/fmc_core.c \
                hcitrans/stk/fm_drv_if.c \
                rx/fm_rx_sm.c \
                rx/fm_rx.c \
                tx/fm_tx.c \
                tx/fm_tx_sm.c

LOCAL_C_INCLUDES = \
                $(LOCAL_PATH)/../../HSW_FMStack/stack \
                $(LOCAL_PATH)/../../HSW_FMStack/stack/inc \
                $(LOCAL_PATH)/../../HSW_FMStack/stack/inc/int \
                $(LOCAL_PATH)/../../MCP_Common/Platform/fmhal/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/fmhal/inc/int \
                $(LOCAL_PATH)/../../MCP_Common/Platform/fmhal/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/bthal/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/fmhal/LINUX/common/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/bthal/LINUX/common/inc \
                $(LOCAL_PATH)/../../MCP_Common/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc \
                $(LOCAL_PATH)/../../MCP_Common/Platform/os/LINUX/common/inc \
                $(LOCAL_PATH)/../../MCP_Common/tran \
                $(LOCAL_PATH)/../../MCP_Common/Platform/inc/$(BTIPS_SDK_VER)_$(BTIPS_TARGET_PLATFORM) \
                $(LOCAL_PATH)/../../MCP_Common/Platform/bthal/inc \
                $(LOCAL_PATH)/../../../fm_chrlib


LOCAL_MODULE:=libfmstack
LOCAL_MODULE_TAGS:= optional

ifeq ($(FM_MCP_STK),1)
LOCAL_PRELINK_MODULE := false
LOCAL_STATIC_LIBRARIES := libccm libfmhal libmcptransport
LOCAL_SHARED_LIBRARIES := liblog libmcphal
#BLUEZ_SOLUTION
LOCAL_SHARED_LIBRARIES += libfmchr
include $(BUILD_SHARED_LIBRARY)
else
include $(BUILD_STATIC_LIBRARY)
endif

endif
