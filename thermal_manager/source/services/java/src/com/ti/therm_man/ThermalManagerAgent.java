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

import android.app.Activity;
import android.util.Log;

import android.content.Context;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;

/**
* @brief
* ThermalManagerAgent is the main entry point to the ThermalManagerDaemon
*
*/
public class ThermalManagerAgent implements ServiceConnection {

    private static final String TAG = "ThermalManagerAgent";
    private IBinder mServiceToken;
    private IThermalManagerService mService;
    private static Context mContext;
    private boolean mConnected;
    private static ThermalManagerAgentConnection mClient;

    /**
    * Constructor[s]
    */
    public ThermalManagerAgent(Context context, ThermalManagerAgentConnection client) {
        mContext = context;
        mClient = client;
        mService = null;
        mServiceToken = null;
        mConnected = false;
    }

    /**
    *
    * Creates a connection to the ThermalManagerService.
    * (System call by Android! no direct invocation)
    *
    */
    public void onServiceConnected(ComponentName className, IBinder service) {
        synchronized( this ) {
            mService = IThermalManagerService.Stub.asInterface((IBinder)service);
            mServiceToken = (IBinder)service;
            mConnected = true;
            if (mClient != null)
                mClient.onAgentConnected();
        }
    }

    /**
    *
    * Releases the connection to the ThermalManagerDaemon.
    * (System call by Android! no direct invocation)
    *
    */
    public void onServiceDisconnected(ComponentName className) {
        synchronized( this ) {
            mClient.onAgentDisconnected();
            mService = null;
            mServiceToken = null;
            mConnected = false;
        }
    }

    /** Creates a connection to the ThermalManagerDaemon. */
    public void Connect() {
        synchronized( this ) {
            if (!mConnected) {
                Intent intent = new Intent("com.ti.therm_man.IThermalManagerService");
                if (!mContext.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                    Log.e(TAG, " Failed to launch intent connecting to IThermalManagerService");
                } else {
                    mConnected = true;
                    Log.e(TAG, " Connect():: TRUELY connected!");
                }
            }
            else
            {
                Log.d(TAG, "Already connected!");
            }
        }
    }

    /**
    *
    * Releases the connection to the ThermalManagerDaemon.
    *
    */
    public void Disconnect() {
        synchronized (this) {
            if (mConnected) {
                try {
                    mContext.unbindService(this);
                } catch (IllegalArgumentException ex) {
                    Log.v(TAG, " Could not disconnect from service: "+ex.toString());
                }
                mConnected = false;
            } else {
                Log.d(TAG, "Already disconnected!");
            }
        }
    }

    /**
    *
    * Returns whether we are connected to the ThermalManagerDaemon service
    * @return true if we are connected, false otherwise
    */
    public synchronized boolean isConnected() {
        return (mConnected);
    }

    /**
    * Defines a power state for the USP's current profile.
    *
    * @param state  - power state to configure USP with
    */
    public void SetThermalState(String value)
    {
        if (mService != null) {
            try {
                mService.setThermalState(value);
            } catch (RemoteException e) {
                Log.e(TAG, " Remote call exception: " + e.toString());
            } catch (NullPointerException e2) {
                Log.e(TAG, " Null pointer exception: "+ e2.toString());
            }
        }
    }

    public void InitThermalState()
    {
        if (mService != null) {
            try {
                mService.initThermalState();
            } catch (RemoteException e) {
                Log.e(TAG, " Remote call exception: " + e.toString());
            } catch (NullPointerException e2) {
                Log.e(TAG, " Null pointer exception: "+ e2.toString());
            }
        }
    }

    //**************************************************************************
    // INTERFACES
    //**************************************************************************

    /**
    *
    *  A callback interface that allows the applications (clients
    *  of the ThermalManagerAgent) to operate on the ThermalManager Service, i.e.
    *  connect to / disconnect from it.
    *
    */
    public interface ThermalManagerAgentConnection {

        /** Called when the ThermalManagerAgent connects to the ThermalManagerService */
        public void onAgentConnected();

        /** Called when the ThermalManagerDaemon changes the Power State */
        public void onPowerStateChange(int newState);

        /** Called when the ThermalManagerAgent disconnects from the ThermalManagerService */
        public void onAgentDisconnected();
    }
}

