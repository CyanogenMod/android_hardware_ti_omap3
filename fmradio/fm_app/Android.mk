BUILD_FMAPP:=1

FM_STACK_PATH:= $(call my-dir)/../fm_stack/
LOCAL_PATH := $(call my-dir)
ALSA_PATH := $(call my-dir)/../../alsa-lib-1.0.13/

include $(CLEAR_VARS)

ifeq ($(BUILD_FMAPP),1)

#
# FM Application
#


LOCAL_C_INCLUDES:=\
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc/int \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc 	\
	$(FM_STACK_PATH)/MCP_Common/Platform/os/Linux 	\
	$(FM_STACK_PATH)/MCP_Common/Platform/inc 		\
	$(FM_STACK_PATH)/MCP_Common/tran 	\
	$(FM_STACK_PATH)/MCP_Common/inc 	\
	$(FM_STACK_PATH)/HSW_FMStack/stack/inc/int \
	$(FM_STACK_PATH)/HSW_FMStack/stack/inc 	\
	external/bluez/libs/include 	\
	$(FM_STACK_PATH)/fm_app		\
	$(ALSA_PATH)/include 


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
fm_app.c fm_trace.c						

LOCAL_SHARED_LIBRARIES := \
	libbluetooth libasound libfm_stack  

LOCAL_STATIC_LIBRARIES := 

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:=fmapp

include $(BUILD_EXECUTABLE)

endif # BUILD_FMAPP_
