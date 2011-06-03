########################################################################
 # Copyright (C) 2011, Linaro Limited.
 #
 # This file is part of libthermal.
 #
 # All rights reserved. This program and the accompanying materials
 # are made available under the terms of the Eclipse Public License v1.0
 # which accompanies this distribution, and is available at
 # http://www.eclipse.org/legal/epl-v10.html
 #
 # Contributors:
 #     Steve Jahnke <steve.jahnke@linaro.org> (Texas Instruments)
 #     Hector Barajas <barajas@ti.com>
########################################################################

ifeq ($(TARGET_BOARD_PLATFORM),omap4)

LOCAL_PATH:= $(call my-dir)

LOCAL_MODULE_TAGS := optional

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= scanner.c
LOCAL_MODULE:= libscanner
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= grammar.c
LOCAL_MODULE:= libgrammar
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= libconfig.c
LOCAL_MODULE:= libconfig
LOCAL_MODULE_TAGS := optional

LOCAL_STATIC_LIBRARIES := \
		libscanner \
		libgrammar \

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

endif
