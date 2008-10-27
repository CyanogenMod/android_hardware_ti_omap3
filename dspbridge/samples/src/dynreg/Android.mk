LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	dynreg.c \
	DLsymtab.c \
	DLsymtab_support.c \
	cload.c \
	getsection.c \
	reloc.c \
	csl.c \
	uuidutil.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../inc \
	$(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := \
	libbridge
	
LOCAL_CFLAGS += -MD -pipe  -fomit-frame-pointer -Wall  -Wno-trigraphs -Werror-implicit-function-declaration  -fno-strict-aliasing -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -msoft-float -Uarm -DMODULE -D__LINUX_ARM_ARCH__=7  -fno-common -DLINUX -DTMS32060 -D_DB_TIOMAP -DOMAP_3430

LOCAL_MODULE:= dynreg.out

include $(BUILD_EXECUTABLE)

