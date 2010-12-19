ifeq ($(TARGET_ARCH),arm)

BTIPS_DEBUG?=0
BTIPS_TARGET_PLATFORM?=zoom2

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -DANDROID -W -Wall 

ifeq ($(PLATFORM_VERSION),1.6)
    BTIPS_SDK_VER=1.6
else
    BTIPS_SDK_VER=2.0
endif

ifeq ($(BTIPS_DEBUG),1)
    LOCAL_CFLAGS+= -g -O0
    LOCAL_LDFLAGS+= -g
else
    LOCAL_CFLAGS+= -O2 -DEBTIPS_RELEASE
endif

LOCAL_SRC_FILES:= \
		mcp_hci_adapt.c \
		mcp_IfSlpMng.c \
		mcp_transport.c \
		mcp_txnpool.c \
		mcp_uartBusDrv.c \
		TxnQueue.c

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/../inc \
		$(LOCAL_PATH)/../Platform/os/LINUX/common/inc \
		$(LOCAL_PATH)/../Platform/os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc \
		$(LOCAL_PATH)/../Platform/inc \
		$(LOCAL_PATH)/../tran \

LOCAL_MODULE := libmcptransport
LOCAL_MODULE_TAGS:=optional

include $(BUILD_STATIC_LIBRARY)

endif
