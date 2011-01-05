BUILD_FMAPP:=1

FM_STACK_PATH:= $(call my-dir)/../fm_stack/
LOCAL_PATH := $(call my-dir)
ALSA_PATH := external/alsa-lib/

include $(CLEAR_VARS)

ifeq ($(BUILD_FMAPP),1)

#
# FM Application
#

LOCAL_C_INCLUDES:=\
 	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/LINUX/common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/LINUX/OMAP2430/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc/int \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/common \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/OMAP2430/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/inc \
	$(FM_STACK_PATH)/MCP_Common/tran \
	$(FM_STACK_PATH)/MCP_Common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/android_zoom2/inc \
	$(FM_STACK_PATH)/HSW_FMStack/stack/inc/int \
 	$(FM_STACK_PATH)/HSW_FMStack/stack/inc \
	$(FM_STACK_PATH)/fm_app	\
	$(ALSA_PATH)/include

LOCAL_CFLAGS:= -g -c -W -Wall -O2 -D_POSIX_SOURCE

ifdef FM_CHR_DEV_ST
LOCAL_C_INCLUDES+= $(FM_STACK_PATH)/../fm_chrlib
LOCAL_CFLAGS+= -DFM_CHR_DEV_ST
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)), omap3)
LOCAL_CFLAGS+= -DTARGET_BOARD_OMAP3
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM)), omap4)
LOCAL_CFLAGS+= -DTARGET_BOARD_OMAP4
endif

LOCAL_SRC_FILES:= \
fm_app.c fm_trace.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth libasound libaudio libfmstack libmcphal

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:=fmapp

include $(BUILD_EXECUTABLE)

endif # BUILD_FMAPP
