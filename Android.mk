# make sure the omap3 HAL code doesn't get picked up by non-omap boards
ifdef OMAP_ENHANCEMENT
include $(call first-makefiles-under,$(call my-dir))
endif
