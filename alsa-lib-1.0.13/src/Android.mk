LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(TARGET_SIMULATOR),true)
#
# libasound
#
LOCAL_SRC_FILES:= \
	async.c conf.c confmisc.c dlmisc.c error.c \
	input.c names.c output.c shmarea.c socket.c userfile.c \
	mixer/bag.c mixer/mixer.c  mixer/simple_abst.c mixer/simple.c mixer/simple_none.c \
	rawmidi/rawmidi.c rawmidi/rawmidi_hw.c rawmidi/rawmidi_symbols.c rawmidi/rawmidi_virt.c \
	timer/timer.c timer/timer_hw.c timer/timer_query.c timer/timer_query_hw.c timer/timer_symbols.c \
	alisp/alisp.c \
	hwdep/hwdep.c hwdep/hwdep_hw.c hwdep/hwdep_symbols.c \
	instr/fm.c instr/iwffff.c instr/simple.c \
	pcm/ffs.c pcm/atomic.c pcm/mask.c pcm/interval.c pcm/pcm.c  \
	pcm/pcm_params.c pcm/pcm_simple.c pcm/pcm_hw.c pcm/pcm_misc.c \
	pcm/pcm_mmap.c pcm/pcm_symbols.c \
	pcm/pcm_generic.c pcm/pcm_plugin.c \
	pcm/pcm_linear.c \
	pcm/pcm_route.c \
	pcm/pcm_mulaw.c \
	pcm/pcm_alaw.c \
	pcm/pcm_rate.c pcm/pcm_rate_linear.c \
	pcm/pcm_lfloat.c \
	pcm/pcm_adpcm.c \
	pcm/pcm_plug.c pcm/pcm_copy.c\
	seq/seq.c seq/seq_event.c seq/seq_hw.c seq/seqmid.c seq/seq_midi_event.c seq/seq_symbols.c \
	control/cards.c control/control.c control/control_ext.c control/control_hw.c control/control_shm.c \
	control/control_symbols.c control/hcontrol.c control/setup.c \
	vsscanf.c
# pulled vsscan.c over from bionic because it was left out 
# include it here as a temp hack to get things to compile
# take out once vsscanf is put into libc

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/pcm \
	$(LOCAL_PATH)/mixer \
	$(LOCAL_PATH)/hwdep \
	$(LOCAL_PATH)/seq \
	$(LOCAL_PATH)/control \
	$(LOCAL_PATH)/alisp \
	system/bionic/include \
	$(TOPDIR)bionic/libc \
	$(TOPDIR)bionic/libc/stdio 
# previous 2 lines are part of the vsscanf hack - remove once put in libc

LOCAL_SHARED_LIBRARIES += \
	libutils \
	libcutils

LOCAL_LDLIBS := -lpthread

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libasound

include $(BUILD_SHARED_LIBRARY)

#
# copy configure files
#
define auto-copy-files
$(if $(filter %: :%,$(1)), \
  $(error $(LOCAL_PATH): Leading or trailing colons in "$(1)")) \
$(foreach t,$(1), \
  $(eval include $(CLEAR_VARS)) \
  $(eval LOCAL_MODULE_TAGS := user development) \
  $(eval LOCAL_MODULE_CLASS := ETC) \
  $(eval LOCAL_SRC_FILES := $(t)) \
  $(eval tw := $(subst /, ,$(strip $(t)))) \
  $(if $(word 4,$(tw)),$(error $(LOCAL_PATH): Bad prebuilt filename '$(t)')) \
  $(if $(word 3,$(tw)), $(eval LOCAL_MODULE := $(word 3,$(tw))), \
    $(eval LOCAL_MODULE := $(word 2,$(tw))) \
   ) \
  $(eval LOCAL_MODULE_PATH := $(2)) \
  $(eval include $(BUILD_PREBUILT)) \
 )
endef

local_target_dir := $(TARGET_OUT)/etc/alsa
copy_files := conf/alsa.conf conf/sndo-mixer.alisp
$(call auto-copy-files, \
    $(copy_files), \
    $(local_target_dir))

copy_files := \
	conf/cards/AACI.conf conf/cards/AU8810.conf conf/cards/Aureon71.conf conf/cards/CS46xx.conf conf/cards/FM801.conf conf/cards/ICH.conf  \
	conf/cards/PC-Speaker.conf conf/cards/SI7018.conf conf/cards/VX222.conf \
	conf/cards/aliases.alisp conf/cards/AU8820.conf conf/cards/CA0106.conf conf/cards/EMU10K1.conf conf/cards/GUS.conf \
	conf/cards/ICH-MODEM.conf conf/cards/PMac.conf conf/cards/TRID4DWAVENX.conf conf/cards/VXPocket440.conf \
	conf/cards/aliases.conf conf/cards/AU8830.conf conf/cards/CMI8338.conf conf/cards/EMU10K1X.conf conf/cards/HDA-Intel.conf \
	conf/cards/Maestro3.conf conf/cards/PMacToonie.conf conf/cards/VIA686A.conf conf/cards/VXPocket.conf \
	conf/cards/ATIIXP.conf conf/cards/Audigy2.conf conf/cards/CMI8338-SWIEC.conf conf/cards/ENS1370.conf conf/cards/ICE1712.conf \
	conf/cards/RME9636.conf conf/cards/VIA8233A.conf conf/cards/YMF744.conf \
	conf/cards/ATIIXP-MODEM.conf conf/cards/Audigy.conf conf/cards/CMI8738-MC6.conf conf/cards/ENS1371.conf conf/cards/ICE1724.conf \
	conf/cards/RME9652.conf conf/cards/VIA8233.conf \
	conf/cards/ATIIXP-SPDMA.conf conf/cards/Aureon51.conf conf/cards/CMI8738-MC8.conf conf/cards/ES1968.conf conf/cards/ICH4.conf \
	conf/cards/NFORCE.conf conf/cards/VIA8237.conf
local_target_dir := $(TARGET_OUT)/etc/alsa/cards
$(call auto-copy-files, \
    $(copy_files), \
    $(local_target_dir))


copy_files := \
	conf/pcm/default.conf conf/pcm/dpl.conf conf/pcm/front.conf conf/pcm/modem.conf conf/pcm/side.conf conf/pcm/surround41.conf\
	conf/pcm/surround51.conf conf/pcm/center_lfe.conf conf/pcm/dmix.conf conf/pcm/dsnoop.conf conf/pcm/iec958.conf \
	conf/pcm/rear.conf conf/pcm/surround40.conf conf/pcm/surround50.conf conf/pcm/surround71.conf
local_target_dir := $(TARGET_OUT)/etc/alsa/pcm
$(call auto-copy-files, \
    $(copy_files), \
    $(local_target_dir))

endif
