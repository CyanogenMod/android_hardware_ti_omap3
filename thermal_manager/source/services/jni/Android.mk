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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    com_ti_therm_ThermalManagerService.cpp

LOCAL_C_INCLUDES := $(TOP)/include
LOCAL_C_INCLUDES += \
    $(JNI_H_INCLUDE)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := \
        libandroid_runtime \
        libcutils \
        libnativehelper \
        libsystem_server \
        libutils \
        libui \
        libthermal_manager

ifeq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_OS),linux)
ifeq ($(TARGET_ARCH),x86)
LOCAL_LDLIBS += -lpthread -ldl -lrt
endif
endif
endif

ifeq ($(WITH_MALLOC_LEAK_CHECK),true)
    LOCAL_CFLAGS += -DMALLOC_LEAK_CHECK
endif

LOCAL_MODULE := libthermal_manJNI

include $(BUILD_SHARED_LIBRARY)

