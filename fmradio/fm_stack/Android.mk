BUILD_FMLIB:=1

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(BUILD_FMLIB),1)

#
# FM Application
#


LOCAL_C_INCLUDES:=\
	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/inc/int \
	$(LOCAL_PATH)/MCP_Common/Platform/fmhal/inc 	\
	$(LOCAL_PATH)/MCP_Common/Platform/os/Linux 	\
	$(LOCAL_PATH)/MCP_Common/Platform/inc 		\
	$(LOCAL_PATH)/MCP_Common/tran 	\
	$(LOCAL_PATH)/MCP_Common/inc 	\
	$(LOCAL_PATH)/HSW_FMStack/stack/inc/int \
	$(LOCAL_PATH)/HSW_FMStack/stack/inc 	\
	external/bluez/libs/include \
	$(LOCAL_PATH)/FM_Trace 


LOCAL_CFLAGS:= -g -c -W -Wall -O2

#Files in Directories which aren't compiled in as of yet!!

#MCP_Common/ccm/im/test/testccmim.c			
#MCP_Common/frame/mcp_gensm.c			
#MCP_Common/frame/mcp_load_manager.c	
#MCP_Common/frame/mcpf_report.c				MCP_Common/ccm/vac/ccm_vaci_debug.c		
#MCP_Common/ccm/vac/ccm_vaci_allocation_engine.c     MCP_Common/ccm/vac/ccm_vaci_mapping_engine.c
#MCP_Common/ccm/cal/ccm_vaci_cal_chip_1273.c  MCP_Common/ccm/cal/ccm_vaci_cal_chip_6450_1_0.c
#MCP_Common/ccm/cal/ccm_vaci_cal_chip_6350.c  
#MCP_Common/ccm/vac/ccm_vaci_configuration_engine.c

LOCAL_SRC_FILES:= \
	MCP_Common/Platform/hw/linux/mcp_hal_pm.c	\
	MCP_Common/Platform/hw/linux/bt_hci_if.c  	\
	MCP_Common/Platform/hw/linux/ccm_hal_pwr_up_dwn.c 	\
	MCP_Common/Platform/os/linux/mcp_hal_os.c	\
	MCP_Common/Platform/fmhal/os/fmc_os.c		\
	MCP_Common/Platform/os/linux/mcp_hal_fs.c      \
	MCP_Common/Platform/os/linux/mcp_hal_log.c	\
   	MCP_Common/Platform/os/linux/mcp_hal_string.c	\
	MCP_Common/Platform/os/linux/mcp_hal_memory.c  	\
	MCP_Common/Platform/os/linux/mcp_win_line_parser.c	\
MCP_Common/Platform/os/linux/mcp_hal_misc.c    	\
MCP_Common/Platform/os/linux/mcp_win_unicode.c		\
MCP_Common/frame/mcp_hci_sequencer.c		\
MCP_Common/frame/mcp_config_reader.c           \
MCP_Common/frame/mcp_config_parser.c		\
MCP_Common/frame/mcp_endian.c			\
MCP_Common/frame/mcp_pool.c			\
MCP_Common/frame/mcpf_main.c                 	\
MCP_Common/frame/mcpf_queue.c			\
MCP_Common/frame/mcp_utils_dl_list.c		\
MCP_Common/frame/mcp_bts_script_processor.c 		\
MCP_Common/frame/mcp_rom_scripts_db.c			\
MCP_Common/ccm/vac/ccm_vac.c                        	\
MCP_Common/ccm/ccm/ccm.c				\
MCP_Common/ccm/cal/ccm_vaci_chip_abstration.c		\
MCP_Common/ccm/im/ccm_im.c				\
MCP_Common/ccm/im/ccm_imi_bt_tran_on_sm.c		\
MCP_Common/ccm/im/ccm_imi_bt_tran_mngr.c		\
MCP_Common/ccm/im/ccm_imi_bt_tran_sm.c			\
MCP_Common/ccm/im/ccm_imi_bt_tran_off_sm.c		\
MCP_Common/init_script/mcp_rom_scripts.c		\
HSW_FMStack/stack/rx/fm_rx.c				\
HSW_FMStack/stack/rx/fm_rx_sm.c			\
HSW_FMStack/stack/tx/fm_tx.c				\
HSW_FMStack/stack/tx/fm_tx_sm.c			\
HSW_FMStack/stack/common/fmc_common.c			\
HSW_FMStack/stack/common/fmc_debug.c  			\
HSW_FMStack/stack/common/fmc_utils.c			\
HSW_FMStack/stack/common/fmc_core.c   			\
HSW_FMStack/stack/common/fmc_pool.c			\
FM_Trace/fm_trace.c						

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_STATIC_LIBRARIES := 

#LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng


LOCAL_PRELINK_MODULE := false
LOCAL_MODULE:=libfm_stack

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_FMAPP_
