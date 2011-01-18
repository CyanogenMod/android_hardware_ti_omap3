ifeq ($(TARGET_ARCH),arm)

BTIPS_DEBUG?=0
BTIPS_TARGET_PLATFORM?=zoom2

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -DANDROID -DMCP_STK_ENABLE -W -Wall

ifeq ($(PLATFORM_VERSION),1.6)
    BTIPS_SDK_VER=1.6
endif	
ifeq ($(PLATFORM_VERSION),2.2)
    BTIPS_SDK_VER=2.2
endif	
BTIPS_SDK_VER?=2.0

ifeq ($(BTIPS_DEBUG),1)
    LOCAL_CFLAGS+= -g -O0
    LOCAL_LDFLAGS+= -g
else
    LOCAL_CFLAGS+= -O2 -DEBTIPS_RELEASE -DMCP_STK_ENABLE
endif

LOCAL_SRC_FILES:= \
                mcp_config_parser.c \
                mcp_config_reader.c \
                mcp_endian.c \
                mcpf_main.c \
                mcpf_queue.c \
                mcpf_report.c \
				mcpf_mem.c \
                mcp_gensm.c \
                mcp_pool.c \
                mcp_rom_scripts_db.c \
                mcp_utils_dl_list.c \
                mcp_utils.c \
                mcp_log_common.c \
                mcp_unicode.c \
				mcpf_msg.c \
				mcpf_services.c \
				mcpf_sll.c \
				mcpf_time.c \
                ../Platform/hw/LINUX/android_$(BTIPS_TARGET_PLATFORM)/mcp_hal_pm.c \
				../Platform/hw/LINUX/pla_hw.c \
				../Platform/os/LINUX/common/mcp_linux_line_parser.c \
				../Platform/os/LINUX/common/mcp_hal_memory.c \
				../Platform/os/LINUX/common/mcp_hal_string.c \
                ../Platform/os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/mcp_hal_os.c \
				../Platform/os/LINUX/common/mcp_hal_misc.c \
				../Platform/os/LINUX/common/mcp_hal_log.c \
				../Platform/os/LINUX/common/mcp_hal_fs.c \
				../Platform/os/LINUX/common/mcp_hal_st.c \
				../Platform/os/LINUX/common/pla_cmdParse.c \
				../Platform/os/LINUX/common/pla_os.c \
				../Platform/os/LINUX/common/mcp_hal_socket.c \
				../Platform/os/LINUX/common/mcp_hal_hci.c \
                ../Platform/init_script/android_$(BTIPS_TARGET_PLATFORM)/mcp_rom_scripts.c



LOCAL_C_INCLUDES := \
		system/core/include \
		$(LOCAL_PATH)/../inc \
		$(LOCAL_PATH)/../Platform/inc \
		$(LOCAL_PATH)/../Platform/inc/int \
		$(LOCAL_PATH)/../Platform/os/LINUX/common/inc \
		$(LOCAL_PATH)/../Platform/os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc \
		$(LOCAL_PATH)/../../NaviLink/SUPLC/Core/include/ti_supl \
		$(LOCAL_PATH)/../../NaviLink/SUPLC/Core/include/ti_client_wrapper

LOCAL_MODULE:=libmcphal
LOCAL_MODULE_TAGS:=optional
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_SHARED_LIBRARY)

endif
