ifeq ($(TARGET_ARCH),arm)

BTIPS_DEBUG?=0
BTIPS_TARGET_PLATFORM?=zoom2

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -DANDROID -DMCP_STK_ENABLE -W -Wall -D_FM_APP

ifeq ($(BTIPS_DEBUG),1)
    LOCAL_CFLAGS+= -g -O0
    LOCAL_LDFLAGS+= -g
else
    LOCAL_CFLAGS+= -O2 -DEBTIPS_RELEASE
endif

LOCAL_SRC_FILES:= \
                ccm/ccm.c \
                ccm/ccm_adapt.c \
                cal/ccm_vaci_cal_chip_6450_1_0.c \
                cal/ccm_vaci_cal_chip_6450_2.c \
                cal/ccm_vaci_chip_abstration.c \
                cal/ccm_vaci_cal_chip_6350.c \
                cal/ccm_vaci_cal_chip_1273.c \
                cal/ccm_vaci_cal_chip_1273_2.c \
                cal/ccm_vaci_cal_chip_1283.c \
                im/ccm_imi_bt_tran_off_sm.c \
                im/ccm_im.c \
                im/ccm_imi_bt_tran_on_sm.c \
                im/ccm_imi_bt_tran_mngr.c \
                im/ccm_imi_bt_tran_sm.c \
                vac/ccm_vaci_allocation_engine.c \
                vac/ccm_vaci_mapping_engine.c \
                vac/ccm_vaci_configuration_engine.c \
                vac/ccm_vaci_debug.c \
				../Platform/hw/LINUX/android_zoom2/ccm_hal_pwr_up_dwn.c \
				../frame/mcp_bts_script_processor.c \
				../frame/mcp_hci_sequencer.c \
				../frame/mcp_load_manager.c

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/../inc \
		$(LOCAL_PATH)/../Platform/inc \
		$(LOCAL_PATH)/../tran \
		$(LOCAL_PATH)/../Platform/os/LINUX/common/inc\
		$(LOCAL_PATH)/../Platform/os/LINUX/android_$(BTIPS_TARGET_PLATFORM)/inc

LOCAL_MODULE:=libccm
LOCAL_MODULE_TAGS:= optional


include $(BUILD_STATIC_LIBRARY)

endif
