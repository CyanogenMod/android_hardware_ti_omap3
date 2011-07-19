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

import com.ti.therm_man.ThermalObserver;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteException;
import android.util.Log;

public class ThermalManagerService extends Service {

    private static final String TAG = "ThermalManagerService";
    // The Thermal Observer which will recieve uEvents
    private ThermalObserver observer;

    // The object which updates the ThermalManager icon in the Status Bar
    private NotificationManager mNotifyMgr;

    // Reference to the system power manager
    protected PowerManager pm;

    // Used to control the service's wakelock
    protected PowerManager.WakeLock serviceWakeLock;

     // Constructor method which sets up any needed initializations.
    public ThermalManagerService() {
        super();
    }

    // Overrides the Service class's standard method. It loads the ThermalObserver
    @Override
    public void onCreate() {

	System.load("/system/lib/libthermal_manJNI.so");
        // use notification manager for status bar notifications (
        //  to indicate the thermal state).
        mNotifyMgr = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        // Let's {responsibly} control the power state
        pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        // get a wake lock when we start
        serviceWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
        // Log.d(TAG,"+ThermalManagerService.create()");
	observer = new ThermalObserver(this);
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Log.d(TAG,"+ThermalManagerService.onBind()");
        if (IThermalManagerService.class.getName().equals(intent.getAction())) {
            return mBinder;
        } else {
            return null;
        }
    }

    @Override
    public boolean onUnbind(Intent intent) {
        // Log.d(TAG,"+ThermalManagerService.onUnbind()");
        if (IThermalManagerService.class.getName().equals(intent.getAction())) {
            return super.onUnbind(intent);
        } else {
            return false;
        }
    }

    @Override
    public void onRebind(Intent intent) {
        // Log.d(TAG,"+ThermalManagerService.onRebind()");
        if (IThermalManagerService.class.getName().equals(intent.getAction())) {
            super.onRebind(intent);
        }
    }

    @Override
    public void onDestroy() {
        // tell the status bar to remove all of our icons and cancel any updates
        mNotifyMgr.cancelAll();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Keep the service running even when no other app is connected
        return START_STICKY;
    }

    public void showNotification(int state) {

        CharSequence notificationText = null;
        int notificationIcon = 0;
        mNotifyMgr.cancelAll();
        if (state != 0)
        {
            // Set the icon, scrolling text and timestamp
            Notification notification = new Notification(R.drawable.stat_on, 
                                                         getText(R.string.tm_state_on),                    
                                                         System.currentTimeMillis());

            // Make the notification persistent in the task bar.
            notification.flags |= Notification.FLAG_AUTO_CANCEL;

            // The PendingIntent to launch our activity if the user selects this notification
            PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
                                                                    new Intent(this, com.ti.therm_man.ThermalManagerService.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP), 
                                                                    0);

            // Set the info for the views that show in the notification panel.
            notification.setLatestEventInfo(this, 
                                            getText(R.string.tm_service_label),
                                            getText(R.string.tm_state_on), 
                                            contentIntent);

            // Send the notification.
            try {
                mNotifyMgr.notify(R.string.tm_service_started, notification);
            } catch (Exception e) {
                Log.e(TAG,"Caught exception due to "+ e.toString());
            }
        }
    }

    public void setThermalState(String value) {
        setThermalManagerNative(value);
    }
    public void initThermalState() {
        initThermalManagerNative();
    }
    private native void setThermalManagerNative(String value);
    private native void initThermalManagerNative();

    //**************************************************************************
    // PRIVATE 
    //**************************************************************************

    /** Defines our local RPC instance object which clients use to communicate with us. */
    private final IThermalManagerService.Stub mBinder = new IThermalManagerService.Stub() {
        public void setThermalState(String value) {
		return;
        }
        public void initThermalState() {
		return;
        }
    };

}

