
/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#ifndef SKIAHW_UTILITY_H
#define SKIAHW_UTILITY_H

#include <string.h>
#include <time.h>
#include "SkBitmap.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include <stdio.h>
#include <semaphore.h>
#include <utils/threads.h>
#include <utils/List.h>
#include <sys/types.h>
#include <time.h>

extern "C" {
    #include "OMX_Component.h"
    #include "OMX_IVCommon.h"
}

#define ALIGN_128_BYTE 128

template <class TYPE>
class WatchdogThread : public android::Thread
{
public:
    WatchdogThread(TYPE* caller, void* caller_param, nsecs_t time):
            android::Thread(false),
            mCaller(caller),
            mCallerParam(caller_param),
            mTime(time){};
    virtual ~WatchdogThread(){SkDebugf("~WatchdogThread()");}
    void restart(){
        android::Mutex::Autolock autolock(mLock);
        mRestartTimer.signal();
    }
    virtual bool threadLoop(){
        android::status_t err;
        bool restart = false;
        SkDebugf("WatchdogThread: Entering threadLoop()");
        for(;;){
            android::Mutex::Autolock autolock(mLock);
            // SkDebugf("WatchdogThread: waitRelative %l", mTime);
            err = mRestartTimer.waitRelative(mLock, mTime);
            // SkDebugf("WatchdogThread: waitRelative done %d", err);
            if(err == -ETIMEDOUT){
                break;
            }
        }
        // class that instantiates WatchdogThread must implement a "bool WatchdogCallback(void*)" function
        // i know...not the best practice
        restart = mCaller->WatchdogCallback(mCallerParam);
        return restart;
    }
private:
    android::Condition mRestartTimer;
    TYPE* mCaller;
    void* mCallerParam;
    android::Mutex mLock;
    nsecs_t mTime;
};

template <class CODEC_TYPE, class WATCHDOG_TYPE>
class SkTIJPEGImageCodecList_Item
{
public:
    CODEC_TYPE* Codec;
    android::sp<WATCHDOG_TYPE>  WatchdogTimer;
};

//  gives a way to increment mLoad and automatically decrement when this class instantiation goes out of scope
class AutoTrackLoad
{
public:
    AutoTrackLoad(int& load){
        load++;
        this->load_ptr = &load;
    }
    ~AutoTrackLoad(){
        (*load_ptr)--;
    }
private:
    int* load_ptr;
};

//Utility functions
OMX_COLOR_FORMATTYPE SkBitmapToOMXColorFormat(SkBitmap::Config config);
OMX_COLOR_FORMATTYPE JPEGToOMXColorFormat(int format);

#endif