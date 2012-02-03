#
# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ifeq ($(TARGET_BOARD_PLATFORM),omap3)

LOCAL_PATH:= $(call my-dir)
HARDWARE_TI_OMAP3_BASE:= $(LOCAL_PATH)
OMAP3_DEBUG_MEMLEAK:= false

ifeq ($(OMAP3_DEBUG_MEMLEAK),true)

OMAP3_DEBUG_CFLAGS:= -DHEAPTRACKER
OMAP3_DEBUG_LDFLAGS:= $(foreach f, $(strip malloc realloc calloc free), -Wl,--wrap=$(f))
OMAP3_DEBUG_SHARED_LIBRARIES:= liblog
BUILD_HEAPTRACKED_SHARED_LIBRARY:= hardware/ti/omap3/heaptracked-shared-library.mk
BUILD_HEAPTRACKED_EXECUTABLE:= hardware/ti/omap3/heaptracked-executable.mk

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= heaptracker.c stacktrace.c mapinfo.c
LOCAL_MODULE:= libheaptracker
LOCAL_MODULE_TAGS:= optional
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= tm.c
LOCAL_MODULE:= tm
LOCAL_MODULE_TAGS:= test
include $(BUILD_HEAPTRACKED_EXECUTABLE)

else
BUILD_HEAPTRACKED_SHARED_LIBRARY:=$(BUILD_SHARED_LIBRARY)
BUILD_HEAPTRACKED_EXECUTABLE:= $(BUILD_EXECUTABLE)
endif

include $(all-subdir-makefiles)
endif
