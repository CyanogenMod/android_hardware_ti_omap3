BUILD_FMLIB:=1

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(BUILD_FMLIB),1)

#
# FM Application
#


LOCAL_C_INCLUDES:=\
 	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/LINUX/common/inc \
 	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/LINUX/OMAP2430/inc \
 	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/inc/int \
 	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/inc \
 	$(LOCAL_PATH)/MCP_Common/Platform/os/LINUX/common \
 	$(LOCAL_PATH)/MCP_Common/Platform/os/LINUX/common/inc \
 	$(LOCAL_PATH)/MCP_Common/Platform/os/LINUX/OMAP2430/inc \
 	$(LOCAL_PATH)/MCP_Common/Platform/inc \
 	$(LOCAL_PATH)/MCP_Common/tran \
 	$(LOCAL_PATH)/MCP_Common/inc \
 	$(LOCAL_PATH)/HSW_FMStack/stack/inc/int \
 	$(LOCAL_PATH)/HSW_FMStack/stack/inc \
	$(LOCAL_PATH)/MCP_Common/Platform/inc/int \
	external/bluetooth/bluez/include \
	$(LOCAL_PATH)/FM_Trace 


LOCAL_CFLAGS:= -g -c -W -Wall -O2 -D_POSIX_SOURCE -DANDROID


LOCAL_SRC_FILES:= \
	MCP_Common/Platform/hw/LINUX/OMAP2430/mcp_hal_pm.c 	\
	MCP_Common/Platform/hw/LINUX/OMAP2430/bt_hci_if.c  	\
	MCP_Common/frame/mcp_unicode.c 		\
	MCP_Common/Platform/fmhal/LINUX/common/os/fmc_os.c 	\
	MCP_Common/frame/mcp_log_common.c 	\
	MCP_Common/Platform/os/LINUX/common/mcp_hal_fs.c \
	MCP_Common/Platform/os/LINUX/common/mcp_hal_log.c \
	MCP_Common/Platform/os/LINUX/common/mcp_hal_memory.c \
	MCP_Common/Platform/os/LINUX/common/mcp_hal_string.c \
	MCP_Common/Platform/os/LINUX/common/mcp_linux_line_parser.c \
	MCP_Common/Platform/os/LINUX/OMAP2430/mcp_hal_os.c \
	MCP_Common/Platform/os/LINUX/common/mcp_hal_misc.c \
	MCP_Common/frame/mcp_hci_sequencer.c \
	MCP_Common/frame/mcp_pool.c \
	MCP_Common/frame/mcpf_queue.c \
	MCP_Common/frame/mcp_endian.c \
	MCP_Common/frame/mcpf_main.c \
	MCP_Common/frame/mcp_config_reader.c \
	MCP_Common/frame/mcp_utils_dl_list.c \
	MCP_Common/frame/mcp_rom_scripts_db.c \
	MCP_Common/frame/mcp_bts_script_processor.c \
	MCP_Common/frame/mcpf_report.c \
	MCP_Common/frame/mcp_load_manager.c \
	MCP_Common/frame/mcp_gensm.c \
	MCP_Common/frame/mcp_config_parser.c \
	MCP_Common/Platform/init_script/OMAP2430/mcp_rom_scripts.c \
	HSW_FMStack/stack/common/fmc_debug.c \
	HSW_FMStack/stack/common/fmc_common.c \
	HSW_FMStack/stack/common/fmc_pool.c \
	HSW_FMStack/stack/common/fmc_utils.c \
	HSW_FMStack/stack/common/fmc_core.c \
	HSW_FMStack/stack/rx/fm_rx.c \
	HSW_FMStack/stack/rx/fm_rx_sm.c \
	HSW_FMStack/stack/tx/fm_tx_sm.c \
	HSW_FMStack/stack/tx/fm_tx.c \
	FM_Trace/fm_trace.c \
	MCP_Common/Platform/hw/LINUX/OMAP2430/ccm_hal_pwr_up_dwn.c \
	MCP_Common/ccm/ccm/ccm.c \
	MCP_Common/frame/mcp_utils.c \
	MCP_Common/ccm/im/ccm_im.c \
	MCP_Common/ccm/vac/ccm_vaci_configuration_engine.c \
	MCP_Common/ccm/vac/ccm_vaci_debug.c 

LOCAL_SHARED_LIBRARIES := \
	libbluetooth libcutils

LOCAL_STATIC_LIBRARIES := 

#LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng


LOCAL_PRELINK_MODULE := false
LOCAL_MODULE:=libfm_stack

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_FMAPP_
