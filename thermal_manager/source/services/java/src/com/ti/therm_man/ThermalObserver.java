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

import com.ti.therm_man.ThermalManagerService;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.os.UEventObserver;
import android.util.Log;

/**
 * ThermalObserver monitors for Thermal events/alerts.
 */
public final class ThermalObserver extends UEventObserver {
    private static final String TAG = ThermalObserver.class.getSimpleName();
    private static final boolean LOG = true;
    private ThermalManagerService ThemService;
    private static final String EMIF1_UEVENT_MATCH = "/devices/platform/omap_emif.0";
    private static final String EMIF2_UEVENT_MATCH = "/devices/platform/omap_emif.1";
    private static final String PCB_UEVENT_MATCH =
                             "/devices/platform/i2c_omap.3/i2c-3/3-0048/hwmon/hwmon0";
    private static final String OMAP_CPU_UEVENT_MATCH =
                             "/devices/platform/omap_temp_sensor.0/hwmon/hwmon1";

    public ThermalObserver(Context context) {

    ThemService = new ThermalManagerService();

        startObserving("DEVPATH=" + EMIF1_UEVENT_MATCH);
        startObserving("DEVPATH=" + EMIF2_UEVENT_MATCH);
        startObserving("DEVPATH=" + PCB_UEVENT_MATCH);
        startObserving("DEVPATH=" + OMAP_CPU_UEVENT_MATCH);
        ThemService.initThermalState();
    }

    @Override
    public void onUEvent(UEventObserver.UEvent event) {
         if (LOG) Log.v(TAG, "THERMAL UEVENT: " + event.toString());

        try {
            String module = "unknown";
            String devpath = event.get("DEVPATH");
            if (devpath.compareTo(EMIF1_UEVENT_MATCH) == 0)         module = "lpddr2";
            else if (devpath.compareTo(EMIF2_UEVENT_MATCH) == 0)    module = "lpddr2";
            else if (devpath.compareTo(PCB_UEVENT_MATCH) == 0)      module = "pcb";
            else if (devpath.compareTo(OMAP_CPU_UEVENT_MATCH) == 0) module = "omap_cpu";
            ThemService.setThermalState(module);
        } catch (NumberFormatException e) {
             Log.e(TAG, "Could not parse switch state from event " + event);
        }
    }
}
