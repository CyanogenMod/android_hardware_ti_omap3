# make sure the omap3 HAL code doesn't get picked up by non-omap boards
ifeq ($(OMAP_ENHANCEMENT),true)
include $(call first-makefiles-under,$(call my-dir))
endif
