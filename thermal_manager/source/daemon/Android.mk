#
# Copyright (C) 2011 The Android Open Source Project
# Copyright (C) 2010-2012 Texas Instruments, Inc. All rights reserved.
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

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
    ThermalDaemon.c
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../inc \
    $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := \
        libnotify \
        libhardware_legacy \
        libthermal_manager \
        liblog

LOCAL_LDLIBS := -llibthermal_manager
LOCAL_MODULE := thermaldaemon
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
endif
