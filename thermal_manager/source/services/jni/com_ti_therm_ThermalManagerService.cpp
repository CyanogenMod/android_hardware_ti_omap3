/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2010-2012 Texas Instruments, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ThermalManager"

#include "jni.h"
#include "JNIHelp.h"
#include "android_runtime/AndroidRuntime.h"

#include <utils/misc.h>
#include <utils/Log.h>
#include "../../../include/thermal_manager.h"

#include <stdio.h>

namespace android
{
    static void setThermalState(JNIEnv *env __attribute__ ((unused)),
                              jobject clazz __attribute__ ((unused)),
                              jstring javaString)
    {
            const char *nativeString = env->GetStringUTFChars(javaString, 0);
        LOGI("Setting the thermal state for %s\n", nativeString);
        /* Call C thermal manager */
        thermal_manager_algo(nativeString);
            env->ReleaseStringUTFChars(javaString, nativeString);
    }

    static void initThermalState(JNIEnv *env __attribute__ ((unused)),
                              jobject clazz __attribute__ ((unused)))
    {
        LOGI("Init the thermal manager\n");
        /* Call C thermal manager */
        thermal_manager_init();
    }

    static JNINativeMethod method_table[] = {
        { "setThermalManagerNative", "(Ljava/lang/String;)V", (void *)setThermalState },
        { "initThermalManagerNative", "()V", (void *)initThermalState },
    };

    int register_ThermalManagerService(JNIEnv *env)
    {
        return jniRegisterNativeMethods(env, "com/ti/therm_man/ThermalManagerService",
            method_table, NELEM(method_table));
    }

};

using namespace android;
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("GetEnv failed!");
        return result;
    }
    LOG_ASSERT(env, "Could not retrieve the env!");

    register_ThermalManagerService(env);

    return JNI_VERSION_1_4;
}
