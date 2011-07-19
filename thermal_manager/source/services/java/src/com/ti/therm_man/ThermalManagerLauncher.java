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

package com.ti.therm_man;

import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.Context;
import android.util.Log;
// import android.os.SystemProperties; // { @hide }

public class ThermalManagerLauncher extends BroadcastReceiver {
    private final static String TAG = "ThermalManagerServiceLauncher";

    @Override
    public void onReceive(Context context, Intent intent)
    {
        Log.d(TAG, "ThermalManagerService::Received Intent " + intent.getAction());
        Intent i = new Intent("com.ti.therm_man.ThermalManagerService");
        if (intent.getAction().equals("android.intent.action.BOOT_COMPLETED"))
        {
            Log.d(TAG, "Launching " + i.getAction());
            context.startService(i);
        }
        else if (intent.getAction().equals("android.intent.action.ACTION_SHUTDOWN"))
        {
            Log.d(TAG, "Not allowing it to shut down starting service again " + i.getAction());
            context.startService(i);
        }
    }
}

