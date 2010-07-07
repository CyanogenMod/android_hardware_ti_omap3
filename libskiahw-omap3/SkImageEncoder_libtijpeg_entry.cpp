
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
/**
* @file SkImageEncoder_libtijpeg_entry.cpp
*
* This file implements SkTIJPEGImageEncoderEntry
*
*/

#include "SkImageEncoder_libtijpeg_entry.h"

#define PRINTF SkDebugf

const int DESIRED_LOAD = 1;
const int MAX_DEL_ATTEMPTS = 3;
const unsigned int MAX_ENCODERS = 1;

static SkTIJPEGImageEncoderListWrapper SkTIJPEGImageEncoderList;
static android::Mutex SkTIJPEGImageEncoderListLock;

SkTIJPEGImageEncoderEntry::~SkTIJPEGImageEncoderEntry()
{

}

SkTIJPEGImageEncoderEntry::SkTIJPEGImageEncoderEntry()
{

}

bool SkTIJPEGImageEncoderEntry::WatchdogCallback(void* __item)
{
    bool restart = false;
    android::List<SkTIJPEGImageEncoderList_Item*>::iterator iter;
    SkTIJPEGImageEncoderList_Item* item = reinterpret_cast<SkTIJPEGImageEncoderList_Item*>(__item);

    android::Mutex::Autolock autolock(SkTIJPEGImageEncoderListLock);
    SkDebugf("SkTIJPEGImageEncoderEntry::WatchdogCallback() item=0x%x", item);

    if(item->Encoder->GetLoad() == 0 || item->Encoder->GetDeleteAttempts() >= MAX_DEL_ATTEMPTS)
    {
        if(item->Encoder->GetDeleteAttempts() > MAX_DEL_ATTEMPTS)
            SkDebugf("    Restart attempt limit reached. deleting...");

        // watchdog has expired, delete our reference to encoder object
        SkAutoTDelete<SkTIJPEGImageEncoder> autodelete(item->Encoder);
        // reset the strong pointer, okay to do here since lifetime will last longer than reset
        item->WatchdogTimer.clear();
        // delete item from encoder list
        for(iter = SkTIJPEGImageEncoderList.list.begin(); iter != SkTIJPEGImageEncoderList.list.end(); iter++)
        {
            if(((SkTIJPEGImageEncoderList_Item*)*iter) == item)
            {
                SkTIJPEGImageEncoderList.list.erase(iter);
                break;
            }
        }
    }else{
        // encoder is still doing something, increase delete attempt, restart watchdog
        SkDebugf("  encoder is still doing something, restart watchdog");
        item->Encoder->IncDeleteAttempts();
        restart = true;
    }
    return restart;
}
bool SkTIJPEGImageEncoderEntry::onEncode(SkWStream* stream, const SkBitmap& bm, int quality)
{
    bool result;
    bool itemFound = false;
    android::List<SkTIJPEGImageEncoderList_Item*>::iterator iter;

    { // scope for lock: SkTIJPEGImageEncoderListLock

        // should only need lock when accesing and modifying static list of encoders
        //      - encoder will handle locking its own critical section
        //      - check encoder load in watchdog callback and make sure not to delete encoder while it is working
        android::Mutex::Autolock autolock(SkTIJPEGImageEncoderListLock);

        //TODO: Need algo to select encoder from list
        //      Maybe we can keep list sorted by Load and just pick off from the beginning of the list
        //      Below will work for now as we are not supporting parallel decodes anyways
        for(iter = SkTIJPEGImageEncoderList.list.begin(); iter != SkTIJPEGImageEncoderList.list.end(); iter++)
        {
            SkTIJPEGImageEncoderList_Item* item = static_cast<SkTIJPEGImageEncoderList_Item*>(*iter);
            if(item->Encoder->GetLoad() < DESIRED_LOAD)
            {
                itemFound = true;
                item->WatchdogTimer->restart();
                break;
            }
        }

        if(!itemFound)
        {
            if(SkTIJPEGImageEncoderList.list.empty() || SkTIJPEGImageEncoderList.list.size() < MAX_ENCODERS)
            {
                SkTIJPEGImageEncoderList_Item* type = new SkTIJPEGImageEncoderList_Item;
                type->Encoder =  SkNEW(SkTIJPEGImageEncoder);
                type->WatchdogTimer = new EncoderWatchdog(this, (void*)type, (nsecs_t) 10*1000*1000*1000);
                type->WatchdogTimer->run("Encoder Watchdog", ANDROID_PRIORITY_DISPLAY);
                SkTIJPEGImageEncoderList.list.insert(SkTIJPEGImageEncoderList.list.begin(), type);
                iter = SkTIJPEGImageEncoderList.list.begin();
            } else {
                // oh well, tried our best. just return an encoder from the top of the list
                iter = SkTIJPEGImageEncoderList.list.begin();
                static_cast<SkTIJPEGImageEncoderList_Item*>(*iter)->WatchdogTimer->restart();
            }
        }
    } //SkTIJPEGImageEncoderListLock

    return static_cast<SkTIJPEGImageEncoderList_Item*>(*iter)->Encoder->onEncode(this, stream, bm, quality);
}
